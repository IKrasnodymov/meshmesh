package org.meshmesh.app.link

import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.ClosedReceiveChannelException
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.withTimeoutOrNull
import java.io.IOException

/** A byte pipe to the board: USB serial, a BLE characteristic pair or a TCP bridge. */
interface ByteLink {
    /** Bytes from the board; called on the link's own thread. */
    var onBytes: (ByteArray, Int) -> Unit
    /** The link broke (cable, radio range, the board switched BLE off). */
    var onClosed: (String) -> Unit
    /** Largest command line the board accepts over this link, in UTF-8 bytes without the newline. */
    val maxCommand: Int
    suspend fun write(data: ByteArray)
    fun close()
}

class LinkClosedException(message: String) : IOException(message)

/** A JSON answer cut by a driver log line (USB: "wifi:timeout when WiFi un-init" when Wi-Fi stops). */
class GarbledAnswerException(command: String) : IOException("Повреждённый ответ на «${command.substringBefore(' ')}»")

/** One command at a time over a [ByteLink]: writes the line and waits for its single answer. */
class LineTransport(val link: ByteLink) {
    private val assembler = LineAssembler()
    private val lines = Channel<String>(Channel.UNLIMITED)
    private val mutex = Mutex()
    @Volatile private var closedReason: String? = null
    // A command that timed out may still be answered. The board answers in order, so before the
    // next command its answer is awaited (up to 2 s) and dropped, not taken for the next one.
    private var late = 0
    @Volatile var lastActivity = System.currentTimeMillis()
        private set
    /** Called once when the link closes, with the reason. */
    var onClosed: (String) -> Unit = {}

    init {
        link.onBytes = { bytes, count -> assembler.feed(bytes, count).forEach { lines.trySend(it) } }
        link.onClosed = { reason -> markClosed(reason) }
    }

    @Synchronized private fun markClosed(reason: String) {
        if (closedReason != null) return
        closedReason = reason
        lines.close(LinkClosedException(reason))
        onClosed(reason)
    }

    val closed get() = closedReason != null

    /** Sends [command] and returns its answer line (OK…, ERR…, Commands:… or JSON). */
    suspend fun exchange(command: String, timeoutMs: Long = 15_000, whileWaiting: (suspend () -> Unit)? = null): String =
        mutex.withLock { exchangeLocked(command, timeoutMs, whileWaiting) }

    /** Runs [block] holding the command lock; [Locked.exchange] works inside it (USB baud switching). */
    suspend fun <T> exclusive(block: suspend Locked.() -> T): T = mutex.withLock { Locked().block() }

    inner class Locked {
        suspend fun exchange(command: String, timeoutMs: Long = 5_000) = exchangeLocked(command, timeoutMs, null)
    }

    private suspend fun exchangeLocked(command: String, timeoutMs: Long, whileWaiting: (suspend () -> Unit)?): String {
        closedReason?.let { throw LinkClosedException(it) }
        val bytes = command.toByteArray(Charsets.UTF_8)
        if (command.isBlank()) throw IOException("ERR empty command")
        if (bytes.size > link.maxCommand) throw IOException("ERR command too long for this link (${bytes.size} > ${link.maxCommand} bytes)")
        if (late > 0) {
            try {
                withTimeoutOrNull(2000) { while (late > 0) if (Replies.anyAnswer(lines.receive())) late-- }
            } catch (e: ClosedReceiveChannelException) {
                throw LinkClosedException(closedReason ?: "closed")
            }
            late = 0
        }
        while (true) { // log lines from before this command
            val old = lines.tryReceive()
            if (old.isClosed) throw LinkClosedException(closedReason ?: "closed")
            old.getOrNull() ?: break
        }
        link.write(bytes + '\n'.code.toByte())
        lastActivity = System.currentTimeMillis()
        try {
            return withTimeout(timeoutMs) {
                var answer: String? = null
                while (answer == null) {
                    val line = if (whileWaiting == null) lines.receive() else receiveTicking(whileWaiting)
                    lastActivity = System.currentTimeMillis()
                    when {
                        Replies.answers(command, line) -> answer = line
                        Replies.expectsJson(command) && (line.startsWith("{") || line.startsWith("[")) -> throw GarbledAnswerException(command)
                        else -> Unit // a log line
                    }
                }
                answer
            }
        } catch (e: TimeoutCancellationException) {
            late = 1
            throw IOException("Нет ответа устройства на «${command.substringBefore(' ')}»")
        } catch (e: ClosedReceiveChannelException) {
            throw LinkClosedException(closedReason ?: "closed")
        }
    }

    /** Raw bytes outside a command (USB keep-alive); skipped while a command is running. */
    suspend fun poke(data: ByteArray): Boolean {
        if (!mutex.tryLock()) return false
        try {
            if (closed) return false
            link.write(data)
            lastActivity = System.currentTimeMillis()
            return true
        } finally {
            mutex.unlock()
        }
    }

    private suspend fun receiveTicking(tick: suspend () -> Unit): String {
        while (true) {
            withTimeoutOrNull(400) { lines.receive() }?.let { return it }
            tick()
        }
    }

    fun close(reason: String = "Отключено") {
        link.close()
        markClosed(reason)
    }
}
