package org.meshmesh.app

import android.content.Context
import android.content.Intent
import androidx.core.content.FileProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest

/**
 * Updates of the app itself from the site: app/version.json next to app/meshmesh.apk
 * (written by .github/workflows/pages.yml). The APK is checked by size and SHA-256 and handed to
 * the Android installer, which also checks that it is signed with the same key.
 */
class Updater(private val context: Context) {
    data class Release(val code: Int, val name: String, val url: String, val size: Long, val sha256: String)

    private val file get() = File(context.cacheDir, "updates/meshmesh.apk")

    /** The published release, or null when it is not newer than this build. */
    suspend fun check(): Release? = withContext(Dispatchers.IO) {
        val c = open(SITE + "version.json")
        val r = try { parse(c.inputStream.use { String(it.readBytes()) }) } finally { c.disconnect() }
        r.takeIf { it.code > BuildConfig.VERSION_CODE }
    }

    /** Downloads the APK into the cache; [progress] gets the bytes read so far. */
    suspend fun download(r: Release, progress: (Long) -> Unit): File = withContext(Dispatchers.IO) {
        val out = file.apply { parentFile?.mkdirs(); delete() }
        val digest = MessageDigest.getInstance("SHA-256")
        var read = 0L
        val c = open(r.url)
        try {
            c.inputStream.use { input ->
                out.outputStream().use { output ->
                    val buf = ByteArray(64 * 1024)
                    while (true) {
                        val n = input.read(buf)
                        if (n < 0) break
                        if (read + n > r.size) throw IOException("файл больше ожидаемого")
                        output.write(buf, 0, n); digest.update(buf, 0, n); read += n
                        progress(read)
                    }
                }
            }
        } finally { c.disconnect() }
        val hash = digest.digest().joinToString("") { "%02x".format(it) }
        if (read != r.size || hash != r.sha256) { out.delete(); throw IOException("файл повреждён при загрузке") }
        out
    }

    /** The Android installer for a downloaded APK (the user confirms it). */
    fun installIntent(apk: File): Intent = Intent(Intent.ACTION_VIEW)
        .setDataAndType(FileProvider.getUriForFile(context, "${context.packageName}.files", apk), "application/vnd.android.package-archive")
        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)

    private fun open(url: String): HttpURLConnection {
        val c = URL(url).openConnection() as HttpURLConnection
        c.connectTimeout = 10_000; c.readTimeout = 20_000; c.useCaches = false
        if (c.responseCode != 200) { c.disconnect(); throw IOException("сайт ответил ${c.responseCode}") }
        return c
    }

    companion object {
        const val SITE = "https://ikrasnodymov.github.io/meshmesh/app/"

        fun parse(text: String): Release {
            val j = JSONObject(text)
            val r = Release(j.getInt("code"), j.getString("name"), SITE + j.getString("file"), j.getLong("size"), j.getString("sha256").lowercase())
            if (r.size <= 0 || r.size > 100L * 1024 * 1024 || !Regex("[0-9a-f]{64}").matches(r.sha256) || !Regex("[\\w.-]+\\.apk").matches(j.getString("file")))
                throw IOException("неверное описание версии")
            return r
        }
    }
}
