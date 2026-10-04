package org.meshmesh.app.flash

import android.content.Context
import android.hardware.usb.UsbDevice
import android.os.PowerManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/** Where the board is: the phone's USB port or a computer's (tools/usb_tcp_bridge.py). */
sealed class FlashPort {
    abstract val kind: String
    abstract fun open(context: Context): SerialIo
    class Usb(val device: UsbDevice) : FlashPort() {
        override val kind = "usb"
        override fun open(context: Context): SerialIo = UsbSerialIo(context, device)
    }
    class Tcp(val host: String, val port: Int) : FlashPort() {
        override val kind = "tcp"
        override fun open(context: Context): SerialIo = TcpSerialIo(host, port)
    }
}

/**
 * The firmware of the site (firmware/boards.json and the board's manifest.json, as the browser
 * installer writes them, tools/pages.py) written over USB with the ROM loader: bootloader,
 * partition table, boot_app0 and the application at their offsets. NVS (key, settings, contacts)
 * and LittleFS (history) stay; the language stays too (the plain partitions.bin, no language mark).
 */
class FirmwareUpdate(private val context: Context, private val port: FlashPort, private val env: String,
                     private val onState: (JSONObject) -> Unit) {
    class Target(val env: String, val name: String, val chip: String, val flashSize: Int, val version: String, val parts: List<Pair<String, Int>>)

    private fun state(message: String, progress: Int? = null) =
        onState(JSONObject().put("state", "flashing").put("kind", port.kind).put("message", message).put("progress", progress ?: JSONObject.NULL))

    /** Downloads, writes and restarts the board; throws with a message for the person on failure. */
    suspend fun run(): Target = withContext(Dispatchers.IO) {
        val lock = context.getSystemService(PowerManager::class.java).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "meshmesh:firmware")
        lock.acquire(15 * 60_000L)
        try {
            state("Загрузка прошивки с сайта…")
            val target = target(env)
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
        const val SITE = "https://ikrasnodymov.github.io/meshmesh/firmware/"

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
            if (b.optString("install") == "uf2") throw IOException("${b.optString("name")}: по USB из приложения пока ставятся только платы ESP32; эту обновите с сайта")
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
