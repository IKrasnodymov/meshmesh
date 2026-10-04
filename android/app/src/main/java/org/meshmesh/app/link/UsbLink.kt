package org.meshmesh.app.link

import android.content.Context
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import com.hoho.android.usbserial.driver.CdcAcmSerialDriver
import com.hoho.android.usbserial.driver.Ch34xSerialDriver
import com.hoho.android.usbserial.driver.ProbeTable
import com.hoho.android.usbserial.driver.UsbSerialDriver
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import com.hoho.android.usbserial.util.SerialInputOutputManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.io.IOException

/** USB serial to the board. Control lines as tools/device.py: neither board must see a reset. */
class UsbLink private constructor(private val port: UsbSerialPort, val native: Boolean) : ByteLink {
    override var onBytes: (ByteArray, Int) -> Unit = { _, _ -> }
    override var onClosed: (String) -> Unit = {}
    override val maxCommand = 1000 // src/main.cpp drops lines of 1024 characters
    private val io: SerialInputOutputManager
    @Volatile private var open = true
    @Volatile private var lastWrite = 0L
    @Volatile var baud = 115200
        private set

    init {
        io = SerialInputOutputManager(port, object : SerialInputOutputManager.Listener {
            override fun onNewData(data: ByteArray) = onBytes(data, data.size)
            override fun onRunError(e: Exception) {
                if (open) { open = false; runCatching { port.close() }; onClosed("USB: кабель отключён или порт закрыт") }
            }
        })
        io.readBufferSize = 16384
        io.start()
    }

    override suspend fun write(data: ByteArray) = withContext(Dispatchers.IO) {
        if (!open) throw LinkClosedException("USB закрыт")
        try {
            // USB-UART boards in repeater/room mode sleep between packets; CR wakes them and is
            // ignored, the waking bytes are lost (src/Power.cpp).
            if (!native && System.currentTimeMillis() - lastWrite > 20000) { port.write(WAKE, 1000); delay(50) }
            lastWrite = System.currentTimeMillis()
            port.write(data, 3000)
        } catch (e: IOException) {
            close(); onClosed("USB: ошибка записи"); throw LinkClosedException("USB: ошибка записи")
        }
    }

    /** M9 only (CH340): the firmware switches after its OK and returns to 115200 after 10 s without input. */
    fun setBaud(rate: Int) {
        port.setParameters(rate, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
        baud = rate
    }

    override fun close() {
        if (!open) return
        open = false
        runCatching { io.listener = null; io.stop() }
        runCatching { port.close() }
    }

    companion object {
        const val M9_VID = 0x1A86
        const val ESP_VID = 0x303A
        const val ESP_NATIVE_PID = 0x1001
        const val NRF_VID = 0x239A // Adafruit nRF52 core (GAT562): TinyUSB CDC, sends nothing without DTR
        const val NRF_APP_PID = 0x8029
        private val WAKE = "\r\r\r\r".toByteArray()

        private val prober by lazy {
            val table = ProbeTable()
            table.addProduct(M9_VID, 0x7522, Ch34xSerialDriver::class.java)
            table.addProduct(M9_VID, 0x7523, Ch34xSerialDriver::class.java)
            table.addProduct(ESP_VID, ESP_NATIVE_PID, CdcAcmSerialDriver::class.java)
            table.addProduct(NRF_VID, NRF_APP_PID, CdcAcmSerialDriver::class.java)
            UsbSerialProber(table)
        }

        fun driverFor(device: UsbDevice): UsbSerialDriver? =
            prober.probeDevice(device) ?: UsbSerialProber.getDefaultProber().probeDevice(device)
                ?: if (device.vendorId == NRF_VID) CdcAcmSerialDriver(device) else null // its serial DFU bootloader

        /** The Adafruit nRF52 bootloader: product IDs without the application's 0x8000 bit. */
        fun isNrfBootloader(device: UsbDevice) = device.vendorId == NRF_VID && device.productId and 0x8000 == 0

        fun isNative(device: UsbDevice) = device.vendorId == ESP_VID && device.productId == ESP_NATIVE_PID

        fun describe(device: UsbDevice): String = when {
            device.vendorId == M9_VID -> "ThinkNode M9 (CH340)"
            isNative(device) -> "Heltec V4 (USB ESP32-S3)"
            isNrfBootloader(device) -> "Загрузчик nRF52 (DFU)"
            device.vendorId == NRF_VID -> "GAT562 (USB nRF52840)"
            else -> device.productName ?: "USB-устройство ${"%04X:%04X".format(device.vendorId, device.productId)}"
        }

        fun serialDevices(context: Context): List<UsbDevice> {
            val usb = context.getSystemService(UsbManager::class.java)
            return usb.deviceList.values.filter { driverFor(it) != null }.sortedBy { it.deviceName }
        }

        fun open(context: Context, device: UsbDevice): UsbLink {
            val usb = context.getSystemService(UsbManager::class.java)
            val driver = driverFor(device) ?: throw IOException("Это USB-устройство не похоже на плату MeshMesh")
            val connection = usb.openDevice(device) ?: throw IOException("Нет доступа к USB: разрешите его в запросе Android")
            val port = driver.ports.first()
            port.open(connection)
            port.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
            val native = isNative(device)
            // Heltec native USB and nRF52 TinyUSB: DTR on, RTS off; M9 CH340 auto-reset circuit: both off.
            runCatching { port.dtr = native || device.vendorId == NRF_VID }
            runCatching { port.rts = false }
            return UsbLink(port, native)
        }
    }
}
