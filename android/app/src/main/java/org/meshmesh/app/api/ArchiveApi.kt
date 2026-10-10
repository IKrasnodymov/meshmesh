package org.meshmesh.app.api

import android.util.AtomicFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import org.meshmesh.app.Alerts
import java.io.File

/** Keys omit the timestamp: setting the board's clock updates the original message. */
internal class MessageHistory(saved: JSONObject = JSONObject()) {
    private val rows = LinkedHashMap<String, JSONObject>()
    private val limits = HashMap<String, Int>()
    private var previousSnapshot = HashSet<String>()
    init {
        val p = saved.optJSONObject("limits") ?: JSONObject()
        for (id in p.keys()) limits[id] = p.optInt(id, 1024).coerceIn(8, 1024)
        val messages = saved.optJSONArray("messages") ?: JSONArray()
        for (i in 0 until messages.length()) messages.getJSONObject(i).let { rows[key(it)] = it }
        val snapshot = saved.optJSONArray("snapshot") ?: JSONArray()
        for (i in 0 until minOf(snapshot.length(), 64)) previousSnapshot += snapshot.getString(i)
        trim()
    }
    private fun key(m: JSONObject) = listOf(m.optInt("protocol", 2), m.optString("source"),
        m.optString("destination"), m.optLong("session"), m.optLong("id"), m.optString("text")).joinToString("\u0000")
    fun policies(channels: JSONArray) {
        for (i in 0 until channels.length()) {
            val c = channels.getJSONObject(i); val p = c.optJSONObject("policy") ?: continue
            val explicit = p.optInt("app_limit")
            limits[c.optString("id")] = if (explicit in listOf(8, 16, 32)) explicit else if (p.optInt("priority", 1) == 0) 32 else 1024
        }
        trim()
    }
    fun merge(messages: JSONArray) {
        for (i in 0 until messages.length()) {
            val m = JSONObject(messages.getJSONObject(i).toString()); val k = key(m); val old = rows[k]
            if (old == null && k in previousSnapshot) continue
            if (old != null) {
                if (old.optLong("time") > 0 && m.optLong("time") == 0L) m.put("time", old.optLong("time"))
                if (old.optInt("status") == 3) m.put("status", 3)
                m.put("heard", maxOf(old.optInt("heard"), m.optInt("heard")))
            }
            rows[k] = m
        }
        previousSnapshot = (0 until minOf(messages.length(), 64)).mapTo(HashSet()) { key(messages.getJSONObject(it)) }
        trim()
    }
    private fun trim() {
        val kept = HashMap<String, Int>(); val remove = ArrayList<String>()
        for ((k, m) in rows.entries.toList().asReversed()) {
            val id = Alerts.conversation(m); val n = (kept[id] ?: 0) + 1; kept[id] = n
            if (n > (limits[id] ?: 1024)) remove += k
        }
        remove.forEach(rows::remove)
        while (rows.size > 4096) rows.remove(rows.keys.first())
    }
    fun messages() = JSONArray(rows.values.toList())
    fun saved() = JSONObject().put("limits", JSONObject(limits as Map<*, *>)).put("messages", messages()).put("snapshot", JSONArray(previousSnapshot.toList()))
}

/** Every foreground and background fetch passes through this archive, independent of transport. */
class ArchiveApi(private val link: DeviceApi, directory: File, nodeKey: String) : DeviceApi {
    override val kind get() = link.kind
    override val label get() = link.label
    override var onLost: (String) -> Unit
        get() = link.onLost
        set(value) { link.onLost = value }
    private val lock = Mutex()
    private val file: AtomicFile
    private var lastSaved = ""
    private var history: MessageHistory
    private var clockAt = 0L
    private var policyAt = 0L
    init {
        require(nodeKey.matches(Regex("[a-fA-F0-9]{64}")))
        directory.mkdirs()
        // At most 32 devices; connecting to one keeps that archive's lifetime current.
        val target = File(directory, nodeKey.lowercase() + ".json")
        directory.listFiles()?.filter { it.extension == "json" && it != target }
            ?.sortedByDescending { it.lastModified() }?.drop(31)?.forEach { AtomicFile(it).delete() }
        file = AtomicFile(target)
        val saved = runCatching { JSONObject(file.openRead().use { String(it.readBytes(), Charsets.UTF_8) }) }.getOrNull()
        history = MessageHistory(saved ?: JSONObject())
        target.setLastModified(System.currentTimeMillis())
    }
    private suspend fun save() = withContext(Dispatchers.IO) {
        val next = history.saved().toString()
        if (next == lastSaved) return@withContext
        val output = file.startWrite()
        try { output.write(next.toByteArray(Charsets.UTF_8)); file.finishWrite(output); lastSaved = next }
        catch (e: Throwable) { file.failWrite(output); throw e }
    }
    override suspend fun request(method: String, path: String, body: String?): ApiReply {
        val reply = link.request(method, path, body)
        if (reply.status != 200) return reply
        val route = splitPath(path).first
        if (method == "GET" && route == "/api/status" && System.currentTimeMillis() - clockAt >= 1_800_000) {
            val now = System.currentTimeMillis()
            val clock = JSONObject().put("unix", now / 1000).put("source", "phone")
            val update = runCatching { link.request("POST", "/api/command", JSONObject().put("command", "clock $clock").toString()) }.getOrNull()
            if (update?.status == 200) clockAt = now
        }
        if (method == "GET" && route == "/api/messages") return lock.withLock {
            if (System.currentTimeMillis() - policyAt > 30_000) {
                val p = runCatching { link.request("GET", "/api/channels", null) }.getOrNull()
                if (p?.status == 200) {
                    history.policies(JSONObject(p.body).optJSONArray("channels") ?: JSONArray())
                    policyAt = System.currentTimeMillis()
                }
            }
            history.merge(JSONArray(reply.body)); save()
            ApiReply(200, history.messages().toString())
        }
        if (method == "GET" && route == "/api/channels") lock.withLock {
            history.policies(JSONObject(reply.body).optJSONArray("channels") ?: JSONArray())
            policyAt = System.currentTimeMillis(); save()
        }
        if (method == "POST" && route == "/api/channels") policyAt = 0
        return reply
    }
    override fun close() = link.close()
}
