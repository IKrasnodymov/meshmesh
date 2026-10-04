package org.meshmesh.app.flash

import android.content.Context
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.PowerManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import org.json.JSONObject
import org.meshmesh.app.link.UsbLink
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/** Where the board is: the phone's USB port or a computer's (tools/usb_tcp_bridge.py). */
sealed class FlashPort {
    abstract val kind: String
    /** The port of the running firmware (ESP32: the ROM loader answers on the same one). */
    abstract fun open(context: Context): SerialIo
    /** nRF52: the serial DFU bootloader, entered with a 1200-baud touch; [access] asks Android for it. */
    abstract suspend fun nrfBootloader(context: Context, access: suspend (UsbDevice) -> Boolean): SerialIo

    class Usb(device: UsbDevice) : FlashPort() {
        override val kind = "usb"
        private val vid = device.vendorId
        private val pid = device.productId
        private val name = device.deviceName
        private fun devices(context: Context) = context.getSystemService(UsbManager::class.java).deviceList.values

        /** The board's own port: the same USB device, or the one with its IDs after a restart. */
        fun appDevice(context: Context): UsbDevice? =
            devices(context).filter { it.vendorId == vid && it.productId == pid }.let { l -> l.firstOrNull { it.deviceName == name } ?: l.firstOrNull() }

        override fun open(context: Context): SerialIo = UsbSerialIo(context, appDevice(context) ?: throw IOException("Плата по USB не найдена"))

        override suspend fun nrfBootloader(context: Context, access: suspend (UsbDevice) -> Boolean): SerialIo {
            var boot = devices(context).firstOrNull(UsbLink::isNrfBootloader) // already there after a failed update
            if (boot == null) {
                val app = appDevice(context) ?: throw IOException("Плата по USB не найдена")
                UsbSerialIo(context, app).use { it.setBaud(1200); it.setDtr(true); Thread.sleep(100); it.setDtr(false); Thread.sleep(100) }
                for (i in 0 until 40) { delay(500); boot = devices(context).firstOrNull(UsbLink::isNrfBootloader); if (boot != null) break }
                boot ?: throw IOException("Плата не вошла в загрузчик: дважды быстро нажмите на ней RESET и повторите")
            }
            if (!access(boot)) throw IOException("Нет доступа к загрузчику платы: разрешите USB в запросе Android")
            return UsbSerialIo(context, boot).also { it.setDtr(true) }
        }
    }

    class Tcp(val host: String, val port: Int) : FlashPort() {
        override val kind = "tcp"
        override fun open(context: Context): SerialIo = TcpSerialIo(host, port)
        override suspend fun nrfBootloader(context: Context, access: suspend (UsbDevice) -> Boolean): SerialIo =
            TcpSerialIo(host, port).also { io -> runCatching { io.nrfBootloader() }.onFailure { io.close(); throw it } }
    }
}

/**
 * The firmware of the site (firmware/boards.json and the board's manifest.json, as the browser
 * installer writes them, tools/pages.py) written over USB with the ROM loader: bootloader,
 * partition table, boot_app0 and the application at their offsets. NVS (key, settings, contacts)
 * and LittleFS (history) stay; the language stays too (the plain partitions.bin, no language mark).
 */
class FirmwareUpdate(private val context: Context, private val port: FlashPort, private val env: String,
                     private val access: suspend (UsbDevice) -> Boolean, private val onState: (JSONObject) -> Unit) {
    /** ESP32: [parts] with their offsets; nRF52: [dfu], the application and its init packet (site paths). */
    class Target(val env: String, val name: String, val chip: String, val flashSize: Int, val version: String,
                 val parts: List<Pair<String, Int>>, val dfu: Pair<String, String>? = null)

    private fun state(message: String, progress: Int? = null) =
        onState(JSONObject().put("state", "flashing").put("kind", port.kind).put("message", message).put("progress", progress ?: JSONObject.NULL))

    /** Downloads, writes and restarts the board; throws with a message for the person on failure. */
    suspend fun run(): Target = withContext(Dispatchers.IO) {
        val lock = context.getSystemService(PowerManager::class.java).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "meshmesh:firmware")
        lock.acquire(15 * 60_000L)
        try {
            state("Загрузка прошивки с сайта…")
            val target = target(env)
            target.dfu?.let { (bin, dat) -> nrf(get(ROOT + bin), get(ROOT + dat)); return@withContext target }
            val parts = target.parts.mapIndexed { i, (path, offset) ->
                state("Загрузка ${path} (${i + 1}/${target.parts.size})…")
                EspLoader.Part(offset, get(SITE + "${target.env}/$path"), path)
            }
            check(parts)
            state("Перевод платы в загрузчик…")
            port.open(context).use { io ->
                val loader = EspLoader(io) { state(it) }
                loader.connect()
                if (loader.chip?.title != target.chip) throw IOException("На плате ${loader.chip?.title}, а прошивка для ${target.chip}: обновление отменено")
                loader.changeBaud(460800)
                loader.flash(parts, target.flashSize) { what, done, total -> state(what, (done * 100 / total).toInt()) }
                state("Перезапуск платы…", 100)
                loader.reset()
            }
            target
        } finally {
            if (lock.isHeld) lock.release()
        }
    }

    /** nRF52: the application only, by the bootloader's serial DFU; the language field stays "--" (kept). */
    private suspend fun nrf(image: ByteArray, init: ByteArray) {
        if (!NrfDfu.initMatches(image, init) || image.size < 100_000 || image.size > 0xC0000)
            throw IOException("Файлы прошивки с сайта не сходятся: обновление отменено")
        state("Перевод платы в загрузчик…")
        port.nrfBootloader(context, access).use { io -> NrfDfu.flash(io, image, init) { what, p -> state(what, p) } }
        state("Запуск новой прошивки…", 100)
    }

    /** The files must be what the site's installer writes: DIO bootloader, partition table, application. */
    private fun check(parts: List<EspLoader.Part>) {
        for (p in parts) {
            val d = p.data
            val ok = when (p.name) {
                "bootloader.bin" -> d.size > 1024 && d[0] == 0xE9.toByte() && d[2].toInt() == 2 // DIO: QIO boot-looped these boards
                "partitions.bin" -> d.size <= 0x1000 && d[0] == 0xAA.toByte() && d[1] == 0x50.toByte()
                "boot_app0.bin" -> d.size == 8192
                "firmware.bin" -> d.size > 100_000 && d[0] == 0xE9.toByte()
                else -> false
            }
            if (!ok) throw IOException("Файл ${p.name} с сайта не похож на прошивку: обновление отменено")
        }
    }

    companion object {
        const val ROOT = "https://ikrasnodymov.github.io/meshmesh/"
        const val SITE = ROOT + "firmware/"

        /** The site's build for this board: [env] is status "board"; Heltec V4 R8 reports heltec_v4 too. */
        fun envFor(status: JSONObject): String {
            val board = status.optString("board")
            return if (board == "heltec_v4" && status.optLong("psram") > 3_000_000) "heltec_v4_r8" else board
        }

        fun target(env: String): Target {
            val boards = JSONObject(String(get(SITE + "boards.json")))
            val list = boards.getJSONArray("boards")
            val b = (0 until list.length()).map { list.getJSONObject(it) }.firstOrNull { it.optString("env") == env }
                ?: throw IOException("На сайте нет прошивки для этой платы ($env)")
            b.optJSONObject("dfu")?.let { d ->
                return Target(env, b.optString("name"), b.optString("chip"), 0, boards.optString("version"), emptyList(), d.getString("bin") to d.getString("dat"))
            }
            val manifest = JSONObject(String(get(SITE + "$env/manifest.json")))
            val build = manifest.getJSONArray("builds").getJSONObject(0)
            val parts = build.getJSONArray("parts").let { a -> (0 until a.length()).map { a.getJSONObject(it).getString("path") to a.getJSONObject(it).getInt("offset") } }
            if (parts.map { it.first } != listOf("bootloader.bin", "partitions.bin", "boot_app0.bin", "firmware.bin"))
                throw IOException("Неожиданный состав прошивки на сайте")
            val size = Regex("(\\d+)MB").find(b.optString("flash"))?.groupValues?.get(1)?.toInt() ?: throw IOException("Неизвестный объём flash")
            return Target(env, b.optString("name"), build.getString("chipFamily"), size shl 20, boards.optString("version"), parts)
        }

        private fun get(url: String): ByteArray {
            val c = URL(url).openConnection() as HttpURLConnection
            c.connectTimeout = 10_000; c.readTimeout = 30_000; c.useCaches = false
            try {
                if (c.responseCode != 200) throw IOException("Сайт ответил ${c.responseCode} на ${url.substringAfterLast('/')}")
                return c.inputStream.use { it.readBytes() }
            } catch (e: java.net.UnknownHostException) {
                throw IOException("Нет интернета: прошивка скачивается с сайта")
            } finally { c.disconnect() }
        }
    }
}
