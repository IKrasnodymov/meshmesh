package org.meshmesh.app.flash

import java.io.ByteArrayOutputStream
import java.io.IOException
import java.security.MessageDigest
import java.util.zip.Deflater

/**
 * The ESP32 / ESP32-S3 ROM serial loader, as esptool speaks it without its stub (esptool 5.4:
 * loader.py, targets/esp32s3.py, reset.py): SLIP frames, compressed flash writes of 1 KB blocks
 * and an MD5 check of every written region. Only the given regions are written: NVS and LittleFS
 * stay. Blocking; run it off the main thread.
 */
class EspLoader(private val io: SerialIo, private val log: (String) -> Unit = {}) {
    enum class Chip(val title: String) { ESP32("ESP32"), ESP32S3("ESP32-S3") }
    class Part(val offset: Int, val data: ByteArray, val name: String)

    var chip: Chip? = null
        private set
    private var usbJtag = false
    private val input = ByteArrayOutputStream()
    private var pending = ByteArray(0)

    // Reset into the loader and sync, several times with each reset sequence (esptool --before default-reset).

    fun connect() {
        var last: Exception? = null
        for (attempt in 0 until 8) {
            when {
                io.native -> usbJtagReset()
                else -> classicReset(if (attempt % 2 == 0) 50 else 550)
            }
            Thread.sleep(50)
            io.flushInput(); pending = ByteArray(0)
            try {
                for (i in 0 until 5) if (sync()) { detect(); return }
            } catch (e: IOException) { last = e }
        }
        throw IOException("Плата не вошла в загрузчик" + (last?.message?.let { ": $it" } ?: ""))
    }

    private fun classicReset(delayMs: Long) {
        io.setDtr(false); io.setRts(true)   // IO0 high, EN low: reset
        Thread.sleep(100)
        io.setDtr(true); io.setRts(false)   // IO0 low, EN high: boot into the loader
        Thread.sleep(delayMs)
        io.setDtr(false)
    }

    private fun usbJtagReset() {
        io.setRts(false); io.setDtr(false); Thread.sleep(100)
        io.setDtr(true); io.setRts(false); Thread.sleep(100)
        io.setRts(true); io.setDtr(false); io.setRts(true); Thread.sleep(100)
        io.setDtr(false); io.setRts(false)
    }

    private fun sync(): Boolean {
        val data = byteArrayOf(7, 7, 0x12, 0x20) + ByteArray(32) { 0x55 }
        send(SYNC, data, 0)
        if (receive(SYNC, 100) == null) return false
        repeat(7) { receive(SYNC, 50) } // the loader answers the sync several times
        return true
    }

    private fun detect() {
        val magic = readReg(0x40001000)
        chip = when (magic) {
            0x00F01D83 -> Chip.ESP32
            0x9 -> Chip.ESP32S3
            else -> throw IOException("Неизвестный чип (0x%08X)".format(magic))
        }
        if (chip == Chip.ESP32S3) {
            usbJtag = (readReg(UARTDEV_BUF_NO) and 0xff) == 4
            if (usbJtag) { // the RTC and super watchdogs keep running over USB Serial/JTAG
                writeReg(WDT_PROTECT, WDT_KEY); writeReg(WDT_CONFIG0, 0); writeReg(WDT_PROTECT, 0)
                writeReg(SWD_PROTECT, SWD_KEY); writeReg(SWD_CONF, readReg(SWD_CONF) or (1 shl 31)); writeReg(SWD_PROTECT, 0)
            }
        }
        log("Загрузчик ${chip!!.title}" + if (usbJtag) " (USB)" else "")
    }

    /** The UART boards: a faster rate for the transfer (the ROM takes the new rate and 0). */
    fun changeBaud(rate: Int) {
        if (io.native) return
        check(CHANGE_BAUD, le(rate, 0))
        io.setBaud(rate)
        Thread.sleep(50)
        io.flushInput(); pending = ByteArray(0)
    }

    /**
     * Writes [parts] (the flash is [flashSize] bytes); [progress] gets bytes written and their total.
     * Each region is checked by the loader's MD5 of the flash afterwards.
     */
    fun flash(parts: List<Part>, flashSize: Int, progress: (String, Long, Long) -> Unit) {
        check(SPI_ATTACH, ByteArray(8))
        check(SPI_SET_PARAMS, le(0, flashSize, 0x10000, 0x1000, 0x100, 0xffff))
        val total = parts.sumOf { it.data.size.toLong() }
        var done = 0L
        for (part in parts) {
            val image = part.data.copyOf((part.data.size + 3) and 3.inv()).also { it.fill(0xff.toByte(), part.data.size, it.size) }
            val packed = deflate(image)
            val blocks = (packed.size + BLOCK - 1) / BLOCK
            val eraseSize = (image.size + BLOCK - 1) / BLOCK * BLOCK
            progress("Стирание: ${part.name}", done, total)
            val begin = le(eraseSize, blocks, BLOCK, part.offset) + if (chip == Chip.ESP32S3) le(0) else ByteArray(0)
            check(DEFL_BEGIN, begin, timeout = perMb(30_000, eraseSize))
            for (seq in 0 until blocks) {
                val block = packed.copyOfRange(seq * BLOCK, minOf(packed.size, (seq + 1) * BLOCK))
                var tries = 0
                while (true) {
                    try { check(DEFL_DATA, le(block.size, seq, 0, 0) + block, checksum(block), 10_000); break }
                    catch (e: IOException) { if (++tries >= 3) throw IOException("${part.name}, блок ${seq + 1}/$blocks: ${e.message}") }
                }
                progress("Запись: ${part.name}", done + image.size.toLong() * (seq + 1) / blocks, total)
            }
            progress("Проверка: ${part.name}", done + image.size, total)
            val md5 = String(check(SPI_FLASH_MD5, le(part.offset, image.size, 0, 0), timeout = perMb(8_000, image.size), payload = 32), Charsets.US_ASCII)
            val expected = MessageDigest.getInstance("MD5").digest(image).joinToString("") { "%02x".format(it) }
            if (!md5.equals(expected, ignoreCase = true)) throw IOException("${part.name}: записанное не совпадает с файлом (MD5)")
            done += image.size
        }
    }

    /** Leaves the loader into the new firmware: a watchdog over USB Serial/JTAG, else the EN pin. */
    fun reset() {
        if (chip == Chip.ESP32S3) runCatching { writeReg(FORCE_DOWNLOAD, 0, mask = 1) } // not forced into the loader again
        if (usbJtag) {
            runCatching {
                writeReg(WDT_PROTECT, WDT_KEY); writeReg(WDT_CONFIG1, 2000)
                writeReg(WDT_CONFIG0, (1 shl 31) or (5 shl 28) or (1 shl 8) or 2); writeReg(WDT_PROTECT, 0)
            }
            Thread.sleep(500)
        } else {
            io.setDtr(false); io.setRts(true); Thread.sleep(100); io.setRts(false)
        }
    }

    // Commands.

    private fun readReg(address: Int): Int = command(READ_REG, le(address)).first
    private fun writeReg(address: Int, value: Int, mask: Int = -1) { check(WRITE_REG, le(address, value, mask, 0)) }

    /** Sends a command and returns the data before its status bytes; a nonzero status is an error. */
    private fun check(op: Int, data: ByteArray, sum: Int = 0, timeout: Int = 3000, payload: Int = 0): ByteArray {
        val body = command(op, data, sum, timeout).second
        if (body.size < payload + 2) throw IOException("Короткий ответ загрузчика (команда 0x%02X)".format(op))
        if (body[payload].toInt() != 0) throw IOException("Загрузчик: ошибка 0x%02X в команде 0x%02X".format(body[payload + 1].toInt() and 0xff, op))
        return body.copyOf(payload)
    }

    private fun command(op: Int, data: ByteArray, sum: Int = 0, timeout: Int = 3000): Pair<Int, ByteArray> {
        send(op, data, sum)
        return receive(op, timeout) ?: throw IOException("Загрузчик не ответил (команда 0x%02X)".format(op))
    }

    private fun send(op: Int, data: ByteArray, sum: Int) {
        val packet = byteArrayOf(0, op.toByte(), data.size.toByte(), (data.size shr 8).toByte()) + le(sum) + data
        io.write(Slip.encode(packet))
    }

    /** The response to [op]: its value field and data, or null after [timeoutMs]. */
    private fun receive(op: Int, timeoutMs: Int): Pair<Int, ByteArray>? {
        val end = System.currentTimeMillis() + timeoutMs
        while (true) {
            val (frame, rest) = Slip.next(pending)
            pending = rest
            if (frame != null) {
                if (frame.size >= 8 && frame[0].toInt() == 1 && (frame[1].toInt() and 0xff) == op) {
                    val value = (frame[4].toInt() and 0xff) or ((frame[5].toInt() and 0xff) shl 8) or
                        ((frame[6].toInt() and 0xff) shl 16) or ((frame[7].toInt() and 0xff) shl 24)
                    return value to frame.copyOfRange(8, frame.size)
                }
                continue // another command's late answer or boot text
            }
            val left = end - System.currentTimeMillis()
            if (left <= 0) return null
            pending += io.read(minOf(left, 100L).toInt())
        }
    }

    companion object {
        const val BLOCK = 0x400 // FLASH_WRITE_SIZE of the ROM loader
        private const val SYNC = 0x08; private const val WRITE_REG = 0x09; private const val READ_REG = 0x0A
        private const val SPI_SET_PARAMS = 0x0B; private const val SPI_ATTACH = 0x0D; private const val CHANGE_BAUD = 0x0F
        private const val DEFL_BEGIN = 0x10; private const val DEFL_DATA = 0x11; private const val SPI_FLASH_MD5 = 0x13
        // ESP32-S3 registers (esptool 5.4 targets/esp32s3.py).
        private const val UARTDEV_BUF_NO = 0x3FCEF14C
        private const val RTC = 0x60008000
        private const val WDT_CONFIG0 = RTC + 0x98; private const val WDT_CONFIG1 = RTC + 0x9C
        private const val WDT_PROTECT = RTC + 0xB0; private const val WDT_KEY = 0x50D83AA1
        private const val SWD_CONF = RTC + 0xB4; private const val SWD_PROTECT = RTC + 0xB8; private const val SWD_KEY = 0x8F1D312A.toInt()
        private const val FORCE_DOWNLOAD = 0x6000812C

        fun le(vararg values: Int) = ByteArray(values.size * 4) { (values[it / 4] ushr (8 * (it % 4))).toByte() }
        fun checksum(data: ByteArray) = data.fold(0xEF) { a, b -> a xor (b.toInt() and 0xff) }
        fun deflate(data: ByteArray): ByteArray {
            val d = Deflater(9); d.setInput(data); d.finish()
            val out = ByteArrayOutputStream(); val buf = ByteArray(65536)
            while (!d.finished()) out.write(buf, 0, d.deflate(buf))
            d.end(); return out.toByteArray()
        }
        private fun perMb(ms: Int, size: Int) = maxOf(3000, (ms.toLong() * size / 1_000_000).toInt())
    }
}

/** SLIP framing of the ROM loader: 0xC0 ends, 0xDB 0xDC and 0xDB 0xDD escape 0xC0 and 0xDB. */
object Slip {
    fun encode(packet: ByteArray): ByteArray {
        val out = ByteArrayOutputStream(packet.size + 8)
        out.write(0xC0)
        for (b in packet) when (b.toInt() and 0xff) {
            0xC0 -> { out.write(0xDB); out.write(0xDC) }
            0xDB -> { out.write(0xDB); out.write(0xDD) }
            else -> out.write(b.toInt())
        }
        out.write(0xC0)
        return out.toByteArray()
    }

    /** The first complete frame in [buffer] (decoded) and the bytes after it; bytes before a frame are dropped. */
    fun next(buffer: ByteArray): Pair<ByteArray?, ByteArray> {
        var start = buffer.indexOf(0xC0.toByte())
        while (start >= 0) {
            var end = start + 1
            while (end < buffer.size && buffer[end] != 0xC0.toByte()) end++
            if (end >= buffer.size) return null to buffer.copyOfRange(start, buffer.size)
            if (end == start + 1) { start = end; continue } // two marks in a row: the second starts a frame
            val out = ByteArrayOutputStream()
            var i = start + 1
            while (i < end) {
                val b = buffer[i].toInt() and 0xff
                if (b == 0xDB && i + 1 < end) { out.write(if ((buffer[i + 1].toInt() and 0xff) == 0xDC) 0xC0 else 0xDB); i += 2 }
                else { out.write(b); i++ }
            }
            return out.toByteArray() to buffer.copyOfRange(end + 1, buffer.size)
        }
        return null to ByteArray(0)
    }
}
