package org.meshmesh.app.api

import android.net.Network
import android.util.Base64
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import kotlinx.coroutines.withContext
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/**
 * The device's own HTTP server (src/Portal.cpp) on its access point. [network] is the device's
 * Wi-Fi when the app joined it; the phone's other traffic (map preparation) keeps mobile data.
 */
class HttpApi(
    private val base: String,
    password: String,
    private val network: Network?,
    override val label: String,
) : DeviceApi {
    override val kind = "wifi"
    override var onLost: (String) -> Unit = {}
    private val auth = "Basic " + Base64.encodeToString("meshmesh:$password".toByteArray(), Base64.NO_WRAP)
    private val gate = Semaphore(2) // the ESP32 web server answers one client at a time
    @Volatile private var failures = 0
    @Volatile private var closed = false

    override suspend fun request(method: String, path: String, body: String?): ApiReply = gate.withPermit {
        if (closed) return ApiReply.failed("Отключено")
        withContext(Dispatchers.IO) {
            try {
                val reply = exchange(method, path, body)
                failures = 0
                reply
            } catch (e: IOException) {
                // Three failed requests in a row: the access point is gone (switched off, out of range).
                if (++failures >= 3 && !closed) onLost("Wi-Fi: устройство не отвечает")
                ApiReply.failed(e.message ?: "Нет связи по Wi-Fi")
            }
        }
    }

    fun exchange(method: String, path: String, body: String?): ApiReply {
        val url = URL(base + path)
        val c = (network?.openConnection(url) ?: url.openConnection()) as HttpURLConnection
        try {
            c.connectTimeout = 6000
            c.readTimeout = 25000
            c.useCaches = false
            c.setRequestProperty("Authorization", auth)
            if (method == "POST") {
                c.requestMethod = "POST"
                c.doOutput = true
                c.setRequestProperty("Content-Type", "application/json")
                c.outputStream.use { it.write((body ?: "").toByteArray(Charsets.UTF_8)) }
            }
            val code = c.responseCode
            val bytes = (if (code >= 400) c.errorStream else c.inputStream)?.use { it.readBytes() } ?: ByteArray(0)
            return if (code == 200 && path.startsWith("/api/maps/tile")) ApiReply(code, Base64.encodeToString(bytes, Base64.NO_WRAP), true)
            else ApiReply(code, String(bytes, Charsets.UTF_8))
        } finally {
            c.disconnect()
        }
    }

    override fun close() {
        closed = true
    }
}
