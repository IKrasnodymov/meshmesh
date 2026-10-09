package org.meshmesh.app

import android.content.Context
import org.json.JSONObject

/**
 * App-private settings (no backup, see data_extraction_rules.xml): the last device and the
 * access point passwords. A device draws a new password at each boot; an old one is only a guess.
 */
class Prefs(context: Context) {
    private val p = context.getSharedPreferences("meshmesh", Context.MODE_PRIVATE)

    var last: JSONObject?
        get() = p.getString("last", null)?.let { runCatching { JSONObject(it) }.getOrNull() }
        set(value) { p.edit().putString("last", value?.toString()).apply() }

    var address: String
        get() = p.getString("address", WifiJoin.DEVICE_ADDRESS)!!
        set(value) { p.edit().putString("address", value).apply() }

    var bridge: String
        get() = p.getString("bridge", "10.0.2.2:8771")!!
        set(value) { p.edit().putString("bridge", value).apply() }

    /** Notification settings as the page keeps them (Alerts.parse). */
    var alerts: String?
        get() = p.getString("alerts", null)
        set(value) { p.edit().putString("alerts", value).apply() }

    fun password(ssid: String): String? = p.getString("wifi:$ssid", null)
    fun savePassword(ssid: String, password: String) { p.edit().putString("wifi:$ssid", password).apply() }

    /** What the connection screen shows: last device, saved networks, addresses. */
    fun snapshot(): JSONObject {
        val saved = JSONObject()
        for ((k, v) in p.all) if (k.startsWith("wifi:") && v is String) saved.put(k.removePrefix("wifi:"), v)
        return JSONObject().put("last", last ?: JSONObject.NULL).put("passwords", saved)
            .put("address", address).put("bridge", bridge).put("firmware", BuildConfig.FIRMWARE)
    }
}
