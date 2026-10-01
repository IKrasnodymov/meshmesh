package org.meshmesh.app.api

/** An answer for the web page's fetch(): HTTP status and body (base64 for binary map tiles). */
data class ApiReply(val status: Int, val body: String, val base64: Boolean = false) {
    companion object {
        /** No answer at all: the page's fetch() rejects, as with a dropped Wi-Fi connection. */
        fun failed(message: String) = ApiReply(0, message)
    }
}

/** The device behind the web page, reached over Wi-Fi (HTTP), BLE or USB (command lines). */
interface DeviceApi {
    /** wifi, ble, usb or tcp */
    val kind: String
    /** What the connection card shows: board name and link. */
    val label: String
    var onLost: (String) -> Unit
    suspend fun request(method: String, path: String, body: String?): ApiReply
    fun close()
}

/** Splits "/api/x?a=1&b=2" into the path and its query parameters. */
fun splitPath(path: String): Pair<String, Map<String, String>> {
    val at = path.indexOf('?')
    if (at < 0) return path to emptyMap()
    val query = path.substring(at + 1).split('&').filter { it.isNotEmpty() }.associate {
        val eq = it.indexOf('=')
        val key = if (eq < 0) it else it.substring(0, eq)
        val value = if (eq < 0) "" else java.net.URLDecoder.decode(it.substring(eq + 1), "UTF-8")
        key to value
    }
    return path.substring(0, at) to query
}
