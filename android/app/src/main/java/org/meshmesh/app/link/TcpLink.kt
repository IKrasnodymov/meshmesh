package org.meshmesh.app.link

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.IOException
import java.net.InetSocketAddress
import java.net.Socket
import kotlin.concurrent.thread

/**
 * The board's USB command line through a computer (tools/usb_tcp_bridge.py): the same protocol
 * as USB, for the emulator and for boards plugged into a computer on the same network.
 */
class TcpLink private constructor(private val socket: Socket) : ByteLink {
    override var onBytes: (ByteArray, Int) -> Unit = { _, _ -> }
    override var onClosed: (String) -> Unit = {}
    override val maxCommand = 1000
    @Volatile private var open = true

    init {
        thread(name = "meshmesh-tcp", isDaemon = true) {
            val buffer = ByteArray(16384)
            try {
                val input = socket.getInputStream()
                while (true) {
                    val n = input.read(buffer)
                    if (n < 0) break
                    onBytes(buffer, n)
                }
            } catch (_: IOException) {
            }
            if (open) { open = false; runCatching { socket.close() }; onClosed("Мост USB на компьютере закрыл соединение") }
        }
    }

    override suspend fun write(data: ByteArray): Unit = withContext(Dispatchers.IO) {
        try {
            socket.getOutputStream().run { write(data); flush() }
        } catch (e: IOException) {
            close(); onClosed("Мост USB: ошибка записи"); throw LinkClosedException("Мост USB: ошибка записи")
        }
    }

    override fun close() {
        if (!open) return
        open = false
        runCatching { socket.close() }
    }

    companion object {
        suspend fun open(host: String, port: Int): TcpLink = withContext(Dispatchers.IO) {
            val socket = Socket()
            socket.connect(InetSocketAddress(host, port), 5000)
            socket.tcpNoDelay = true
            TcpLink(socket)
        }
    }
}
