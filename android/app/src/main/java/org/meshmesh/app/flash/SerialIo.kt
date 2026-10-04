package org.meshmesh.app.flash

import android.content.Context
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import com.hoho.android.usbserial.driver.UsbSerialPort
import org.meshmesh.app.link.UsbLink
import java.io.Closeable
import java.io.IOException
import java.net.InetSocketAddress
import java.net.Socket
import java.net.SocketTimeoutException

/** A serial port with its control lines, for the ESP32 ROM loader ([EspLoader]). Blocking calls. */
interface SerialIo : Closeable {
    /** ESP32-S3 USB Serial/JTAG (Heltec V4): no baud rate, its own reset sequence. */
    val native: Boolean
    fun write(data: ByteArray)
    /** Bytes that arrived within [timeoutMs]; empty when none. */
    fun read(timeoutMs: Int): ByteArray
    fun setDtr(on: Boolean)
    fun setRts(on: Boolean)
    fun setBaud(rate: Int)
    fun flushInput() { while (read(20).isNotEmpty()) Unit }
}

/** The board's USB port on the phone (usb-serial-for-android), opened without its command line. */
class UsbSerialIo(context: Context, device: UsbDevice) : SerialIo {
    private val port: UsbSerialPort
    override val native = UsbLink.isNative(device)
    private val buffer = ByteArray(16384)

    init {
        val usb = context.getSystemService(UsbManager::class.java)
        val driver = UsbLink.driverFor(device) ?: throw IOException("USB-устройство не похоже на плату")
        val connection = usb.openDevice(device) ?: throw IOException("Нет доступа к USB")
        port = driver.ports.first()
        port.open(connection)
        port.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
    }

    override fun write(data: ByteArray) = port.write(data, 3000)
    override fun read(timeoutMs: Int): ByteArray {
        val n = port.read(buffer, timeoutMs.coerceAtLeast(1))
        return if (n > 0) buffer.copyOf(n) else ByteArray(0)
    }
    override fun setDtr(on: Boolean) { port.dtr = on }
    override fun setRts(on: Boolean) { port.rts = on }
    override fun setBaud(rate: Int) = port.setParameters(rate, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
    override fun close() { runCatching { port.close() } }
}

/**
 * The same through tools/usb_tcp_bridge.py on a computer (the emulator): after the line "MMRAW1"
 * and its answer the bridge passes bytes and takes frames [type, length u16 LE, payload]: 0 data, 1 DTR and RTS,
 * 2 baud rate u32 LE, 3 nRF52 bootloader (answered "MMBOOT ok" or "MMBOOT fail <why>").
 */
class TcpSerialIo(host: String, port: Int) : SerialIo {
    private val socket = Socket()
    private var dtr = false
    private var rts = false
    private val buffer = ByteArray(16384)
    override val native: Boolean

    init {
        socket.connect(InetSocketAddress(host, port), 5000)
        socket.tcpNoDelay = true
        socket.getOutputStream().write("MMRAW1\n".toByteArray())
        // The bridge answers "MMRAW1 native" (USB Serial/JTAG) or "MMRAW1 uart" (a USB-UART bridge).
        socket.soTimeout = 5000
        val answer = StringBuilder()
        while (true) {
            val c = socket.getInputStream().read()
            if (c < 0 || c == '\n'.code || answer.length > 40) break
            answer.append(c.toChar())
        }
        if (!answer.startsWith("MMRAW1 ")) throw IOException("Мост USB не поддерживает прошивку: обновите tools/usb_tcp_bridge.py")
        native = answer.toString().trim().endsWith("native")
    }

    /** nRF52 through the bridge: it makes the 1200-baud touch and moves to the bootloader's port. */
    fun nrfBootloader() {
        frame(3, ByteArray(0))
        val text = StringBuilder()
        val end = System.currentTimeMillis() + 30_000
        while (System.currentTimeMillis() < end) {
            text.append(String(read(200), Charsets.ISO_8859_1))
            if ("MMBOOT ok\n" in text) return
            Regex("MMBOOT fail ([^\n]*)\n").find(text)?.let { throw IOException("Мост: ${it.groupValues[1]}") }
        }
        throw IOException("Плата не вошла в загрузчик: дважды быстро нажмите на ней RESET и повторите")
    }

    private fun frame(type: Int, payload: ByteArray) {
        val head = byteArrayOf(type.toByte(), (payload.size and 0xff).toByte(), (payload.size shr 8).toByte())
        socket.getOutputStream().apply { write(head + payload); flush() }
    }

    override fun write(data: ByteArray) {
        var at = 0
        while (at < data.size) { val n = minOf(32768, data.size - at); frame(0, data.copyOfRange(at, at + n)); at += n }
    }
    override fun read(timeoutMs: Int): ByteArray {
        socket.soTimeout = timeoutMs.coerceAtLeast(1)
        return try {
            val n = socket.getInputStream().read(buffer)
            if (n < 0) throw IOException("Мост USB закрыл соединение")
            buffer.copyOf(n)
        } catch (_: SocketTimeoutException) { ByteArray(0) }
    }
    private fun lines() = frame(1, byteArrayOf(if (dtr) 1 else 0, if (rts) 1 else 0))
    override fun setDtr(on: Boolean) { dtr = on; lines() }
    override fun setRts(on: Boolean) { rts = on; lines() }
    override fun setBaud(rate: Int) = frame(2, byteArrayOf(rate.toByte(), (rate shr 8).toByte(), (rate shr 16).toByte(), (rate shr 24).toByte()))
    override fun close() { runCatching { socket.close() } }
}
