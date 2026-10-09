package org.meshmesh.app

import android.annotation.SuppressLint
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.bluetooth.BluetoothDevice
import android.content.Intent
import android.content.pm.ServiceInfo
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.net.ConnectivityManager
import android.os.Binder
import android.os.Build
import android.os.IBinder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import org.json.JSONArray
import org.json.JSONObject
import org.meshmesh.app.api.ApiReply
import org.meshmesh.app.api.CommandApi
import org.meshmesh.app.api.DeviceApi
import org.meshmesh.app.api.HttpApi
import org.meshmesh.app.api.TileStore
import org.meshmesh.app.flash.FirmwareUpdate
import org.meshmesh.app.flash.FlashPort
import org.meshmesh.app.link.BleLink
import org.meshmesh.app.link.LineTransport
import org.meshmesh.app.link.TcpLink
import org.meshmesh.app.link.UsbLink
import java.io.File
import java.io.IOException

/**
 * Owns the link to one board. It keeps it while the app is in the background (a foreground
 * service), polls the status there and turns new messages and chess moves into notifications.
 */
class MeshService : Service() {
    inner class Local : Binder() { val service get() = this@MeshService }

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val binder = Local()
    @Volatile var api: DeviceApi? = null
        private set
    private var wifi: WifiJoin.Joined? = null
    private var connecting: Job? = null
    private var background: Job? = null
    private var keepAlive: Job? = null
    private var state = JSONObject().put("state", "idle")
    var onState: (String) -> Unit = {}
    private lateinit var tiles: TileStore
    private lateinit var notifications: NotificationManager
    var uiVisible = true
        set(value) { field = value; schedulePolling() }

    override fun onCreate() {
        super.onCreate()
        tiles = TileStore(File(cacheDir, "tiles"))
        notifications = getSystemService(NotificationManager::class.java)
        notifications.createNotificationChannel(NotificationChannel(CHANNEL_LINK, getString(R.string.channel_link), NotificationManager.IMPORTANCE_LOW))
        notifications.createNotificationChannel(NotificationChannel(CHANNEL_MESSAGES, getString(R.string.channel_messages), NotificationManager.IMPORTANCE_HIGH))
        // Quiet hours and "sound off" of the page's settings: the same notifications without sound and vibration.
        notifications.createNotificationChannel(NotificationChannel(CHANNEL_QUIET, getString(R.string.channel_quiet), NotificationManager.IMPORTANCE_LOW))
    }

    override fun onBind(intent: Intent): IBinder = binder

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_DISCONNECT) disconnect("Отключено")
        else if (api != null) showForeground()
        else stopSelf()
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        disconnect(null)
        scope.cancel()
        super.onDestroy()
    }

    fun stateJson(): String = state.toString()

    private fun publish(name: String, kind: String? = null, message: String? = null, label: String? = null) {
        state = JSONObject().put("state", name).put("kind", kind ?: JSONObject.NULL).put("message", message ?: JSONObject.NULL).put("label", label ?: JSONObject.NULL)
        onState(state.toString())
    }

    suspend fun request(method: String, path: String, body: String?): ApiReply =
        api?.request(method, path, body) ?: ApiReply.failed("Устройство не подключено")

    // Connecting: each path ends in connected (the board answered "status") or failed.

    fun connectUsb(device: UsbDevice) = start("usb", device.deviceName) { stage ->
        stage("Подключение по USB…")
        flashPort = FlashPort.Usb(device)
        val link = UsbLink.open(this, device)
        val transport = LineTransport(link)
        if (!link.native) fastUsb(transport, link)
        val api = CommandApi(transport, "usb", UsbLink.describe(device), tiles,
            recover = if (link.native) null else ({ slowUsb(transport, link) }),
            whileWaiting = if (link.native) ({ transport.link.write(byteArrayOf('\r'.code.toByte())) }) else null)
        api to transport
    }

    @SuppressLint("MissingPermission")
    fun connectBle(device: BluetoothDevice) = start("ble", device.address) { stage ->
        val link = BleLink.open(this, device, stage)
        val transport = LineTransport(link)
        CommandApi(transport, "ble", device.name ?: "MeshMesh BLE", tiles) to transport
    }

    fun connectTcp(host: String, port: Int) = start("tcp", "$host:$port") { stage ->
        stage("Подключение к мосту $host:$port…")
        flashPort = FlashPort.Tcp(host, port)
        val transport = LineTransport(TcpLink.open(host, port))
        CommandApi(transport, "tcp", "USB через компьютер ($host)", tiles) to transport
    }

    fun connectWifi(ssid: String?, password: String, address: String) = start("wifi", ssid ?: address) { stage ->
        // A chosen network, or none yet: Android joins it (its own confirmation). Otherwise the
        // phone is already on a network with this address (joined in settings, or a computer).
        val joined: WifiJoin.Joined?
        val existing = WifiJoin.findNetworkFor(this, address)
        val network = if (address == WifiJoin.DEVICE_ADDRESS && (ssid != null || existing == null)) {
            stage(if (ssid != null) "Подключение к $ssid… Подтвердите сеть в запросе Android" else "Выберите точку MM-… в запросе Android")
            joined = WifiJoin.join(this, ssid, password)
            wifi = joined
            joined.network
        } else {
            joined = null
            existing
        }
        stage("Проверка пароля…")
        val api = HttpApi("http://$address", password, network, ssid ?: address)
        val check = api.request("GET", "/api/config", null)
        if (check.status == 401) throw IOException("Неверный пароль Wi-Fi")
        if (check.status != 200) throw IOException("Устройство не отвечает по адресу $address" + (check.body.takeIf { it.isNotBlank() }?.let { ": $it" } ?: ""))
        joined?.onLost = { api.onLost("Wi-Fi: точка доступа устройства пропала") }
        Prefs(this).savePassword(ssid ?: address, password)
        api to null
    }

    private fun start(kind: String, id: String, open: suspend (stage: (String) -> Unit) -> Pair<DeviceApi, LineTransport?>) {
        disconnect(null)
        flashPort = null
        connecting = scope.launch {
            val stage: (String) -> Unit = { publish("connecting", kind, it) }
            var opened: DeviceApi? = null
            try {
                val (api, transport) = open(stage)
                opened = api
                stage("Чтение состояния…")
                val status = api.request("GET", "/api/status", null)
                if (status.status != 200) throw IOException(status.body.ifBlank { "Устройство не ответило на status" })
                val s = JSONObject(status.body)
                val board = s.optString("board_name").ifBlank { if (s.optString("board") == "heltec_v4") "Heltec V4" else "ThinkNode M9" }
                val node = s.optString("node").takeLast(6)
                val label = "$board · ${s.optString("name").ifBlank { node }}"
                this@MeshService.api = api
                api.onLost = { reason -> scope.launch { lost(api, reason) } }
                Prefs(this@MeshService).last = JSONObject().put("kind", kind).put("id", id).put("label", label)
                publish("connected", kind, null, "$label · ${KIND_TEXT[kind]}")
                rememberCredentials(api, s)
                goForeground()
                schedulePolling()
                if (kind == "usb" && transport != null) keepAliveUsb(api, transport)
            } catch (e: kotlinx.coroutines.CancellationException) {
                if (opened !== api) opened?.close() // cancelled while connecting
                throw e
            } catch (e: Throwable) {
                if (opened !== api) opened?.close()
                wifi?.release(); wifi = null
                publish("failed", kind, e.message ?: e.javaClass.simpleName)
            }
        }
    }

    /** Over USB or BLE the board tells its access point password: Wi-Fi then needs no typing. */
    private suspend fun rememberCredentials(api: DeviceApi, status: JSONObject) {
        if (api.kind == "wifi" || !status.optBoolean("wifi")) return
        val c = runCatching { JSONObject(api.request("GET", "/api/connections", null).body) }.getOrNull() ?: return
        val ssid = c.optString("ssid"); val password = c.optString("password")
        if (ssid.isNotEmpty() && password.isNotEmpty()) Prefs(this).savePassword(ssid, password)
    }

    // M9 over its CH340: 921600 baud for map tiles (tools/maps.py does the same).
    private suspend fun fastUsb(transport: LineTransport, link: UsbLink) {
        val ok = runCatching { transport.exclusive { exchange("baud 921600") } }.getOrNull()?.startsWith("OK") == true
        if (ok) { delay(150); link.setBaud(921600) }
    }

    private suspend fun slowUsb(transport: LineTransport, link: UsbLink): Boolean {
        if (link.baud == 115200) return false
        transport.exclusive { link.setBaud(115200) }
        return true
    }

    /** The M9 returns to 115200 baud after 10 s without input: a carriage return every 3 s idle. */
    private fun keepAliveUsb(api: DeviceApi, transport: LineTransport) {
        keepAlive?.cancel()
        keepAlive = scope.launch {
            while (isActive && this@MeshService.api === api) {
                delay(1000)
                if (System.currentTimeMillis() - transport.lastActivity > 3000) runCatching { transport.poke(byteArrayOf('\r'.code.toByte())) }
            }
        }
    }

    private fun lost(which: DeviceApi, reason: String) {
        if (api !== which) return
        api = null
        which.close()
        stopLink()
        publish("lost", which.kind, reason)
        if (!uiVisible && Alerts(Prefs(this).alerts).lost) notify(ID_LOST, "Связь с устройством потеряна", reason, "")
    }

    // Firmware from the site over the USB link (flash/FirmwareUpdate): the ROM loader takes the port,
    // then the app connects again. A failed write can be repeated: the board stays in its loader.
    private var flashPort: FlashPort? = null
    private var flashEnv: String? = null
    private var flashLang: String? = null
    private var flashing: Job? = null
    val flashingNow get() = flashing?.isActive == true
    /** Set by the activity: Android's access question for a USB device that appears during an update. */
    var askUsb: (suspend (UsbDevice) -> Boolean)? = null

    private suspend fun usbAccess(d: UsbDevice) = getSystemService(UsbManager::class.java).hasPermission(d) || askUsb?.invoke(d) == true

    fun flashFirmware(retry: Boolean) {
        if (flashing?.isActive == true) return
        val port = flashPort ?: return publish("failed", null, "Прошивка ставится только при подключении по USB")
        flashing = scope.launch {
            if (!retry) { flashEnv = null; flashLang = null }
            if (!retry) api?.let { a ->
                runCatching {
                    val status = JSONObject(a.request("GET", "/api/status", null).body)
                    flashEnv = FirmwareUpdate.envFor(status)
                    flashLang = FirmwareUpdate.langFor(status) { JSONObject(a.request("GET", "/api/config", null).body) }
                }
            }
            val env = flashEnv
            if (env.isNullOrEmpty()) return@launch publish("failed", port.kind, "Не удалось определить плату")
            disconnect(null)
            flashPort = port
            try {
                val target = FirmwareUpdate(this@MeshService, port, env, flashLang, ::usbAccess) { s -> state = s; onState(s.toString()) }.run()
                flashEnv = null
                publish("flashing", port.kind, "MeshMesh ${target.version} записана. Подключение…")
                delay(5000)
                when (port) {
                    is FlashPort.Tcp -> connectTcp(port.host, port.port)
                    is FlashPort.Usb -> {
                        // Native USB and nRF52 boards come back as a new USB device after their restart.
                        var d: UsbDevice? = null
                        for (i in 0 until 15) { d = port.appDevice(this@MeshService); if (d != null) break; delay(1000) }
                        if (d != null && usbAccess(d)) connectUsb(d)
                        else publish("idle", "usb", "MeshMesh ${target.version} установлена. Подключите плату снова")
                    }
                }
            } catch (e: kotlinx.coroutines.CancellationException) {
                throw e
            } catch (e: Throwable) {
                state = JSONObject().put("state", "failed").put("kind", port.kind).put("flash", true)
                    .put("message", (e.message ?: e.javaClass.simpleName) + ". Ключ, настройки и история не затронуты; повторите обновление")
                onState(state.toString())
            }
        }
    }

    fun disconnect(reason: String?) {
        connecting?.cancel(); connecting = null
        val old = api
        api = null
        old?.close()
        stopLink()
        if (reason != null) publish("idle", old?.kind, reason)
    }

    private fun stopLink() {
        background?.cancel(); background = null
        keepAlive?.cancel(); keepAlive = null
        wifi?.release(); wifi = null
        runCatching { stopForeground(STOP_FOREGROUND_REMOVE) }
        foreground = false
        stopSelf()
    }

    private var foreground = false

    /** Started once per link; Android then calls onStartCommand, which only shows the notification. */
    private fun goForeground() {
        if (!foreground) runCatching { startForegroundService(Intent(this, MeshService::class.java)) }
        showForeground()
    }

    private fun showForeground() {
        val label = state.optString("label")
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP), PendingIntent.FLAG_IMMUTABLE)
        val stop = PendingIntent.getService(this, 1, Intent(this, MeshService::class.java).setAction(ACTION_DISCONNECT), PendingIntent.FLAG_IMMUTABLE)
        val n = Notification.Builder(this, CHANNEL_LINK)
            .setSmallIcon(R.drawable.ic_stat).setColor(0xFF1FC2AE.toInt())
            .setContentTitle("MeshMesh подключён").setContentText(label)
            .setContentIntent(open).setOngoing(true)
            .addAction(Notification.Action.Builder(null, getString(R.string.disconnect), stop).build())
            .build()
        try {
            startForeground(ID_LINK, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
            foreground = true
        } catch (_: Exception) {
            // Not allowed from the background on this Android: the link still works while the app is open.
        }
    }

    // Background: the page is not polling, so the service does (every 8 s). A change of the RX counter
    // rereads the history and the chess games (else chess every 32 s); incoming messages after the last
    // one seen, chess news (ChessNews) and new nodes become notifications as the page's settings say (Alerts).
    private var lastRx = -1
    private var lastSeen: String? = null
    private var chessGames: JSONArray? = null
    private var knownNodes: Set<String>? = null
    private var channelNames = HashMap<String, String>()

    private fun schedulePolling() {
        background?.cancel(); background = null
        if (uiVisible || api == null) { lastRx = -1; lastSeen = null; chessGames = null; knownNodes = null; return }
        background = scope.launch {
            var round = 0
            while (isActive) {
                val api = api ?: break
                val heard = runCatching { pollStatus(api) }.getOrDefault(false)
                if (heard || round % 4 == 0) runCatching { pollChess(api) }
                round++
                delay(8000)
            }
        }
    }

    /** True when the board received something since the last poll. */
    private suspend fun pollStatus(api: DeviceApi): Boolean {
        val reply = api.request("GET", "/api/status", null)
        if (reply.status != 200) return false
        val status = JSONObject(reply.body)
        val rx = status.optInt("rx", -1)
        if (rx == lastRx) return false
        val first = lastRx < 0
        val messages = api.request("GET", "/api/messages", null)
        if (messages.status != 200) return false
        lastRx = rx
        val incoming = ArrayList<JSONObject>()
        val a = JSONArray(messages.body)
        for (i in 0 until a.length()) a.getJSONObject(i).takeIf { !it.optBoolean("outgoing") }?.let { incoming += it }
        // Without the time: the board fills it in later for messages received before its clock was set.
        val key = { m: JSONObject -> "${m.optLong("session")}|${m.optString("source")}|${m.optLong("id")}|${m.optString("text").hashCode()}" }
        val previous = lastSeen
        lastSeen = incoming.lastOrNull()?.let(key) ?: previous
        if (first || incoming.isEmpty() || lastSeen == previous) return !first
        val start = incoming.indexOfLast { key(it) == previous } + 1
        val fresh = incoming.subList(start.coerceAtLeast(0), incoming.size).takeLast(5)
        val alerts = Alerts(Prefs(this).alerts)
        val me = status.optString("name")
        for (m in fresh) {
            val id = Alerts.conversation(m)
            val how = alerts.forMessage(id, m.optString("text"), me) ?: continue
            val from = m.optString("name").ifBlank { m.optString("source") }
            val title = (if (how == "mention") "@ " else "") + if (Alerts.isChannel(id)) "${channelName(api, id)} · $from" else from
            notify(ID_MESSAGE + (key(m).hashCode() and 0xFFFF), title, m.optString("text"), "chat/$id", alerts)
        }
        if (alerts.nodes) pollNodes(api, alerts) else knownNodes = null
        return true
    }

    private suspend fun channelName(api: DeviceApi, id: String): String {
        if (id == "ALL") return "Public"
        if (id !in channelNames) runCatching {
            val list = JSONObject(api.request("GET", "/api/channels", null).body).optJSONArray("channels")
            for (i in 0 until (list?.length() ?: 0)) list!!.getJSONObject(i).let { channelNames[it.optString("id")] = it.optString("name") }
        }
        return channelNames[id]?.ifBlank { null } ?: "Канал"
    }

    /** Nodes heard for the first time while the app is in the background (after a change of the RX counter). */
    private suspend fun pollNodes(api: DeviceApi, alerts: Alerts) {
        val reply = api.request("GET", "/api/nodes", null)
        if (reply.status != 200) return
        val a = JSONArray(reply.body)
        val ids = (0 until a.length()).map { a.getJSONObject(it) }
        val before = knownNodes
        knownNodes = ids.map { it.optString("id") }.toSet()
        if (before != null) for (p in ids.filter { it.optString("id") !in before })
            notify(ID_NODE + (p.optString("id").hashCode() and 0xFFFF), "Новый узел", p.optString("name").ifBlank { p.optString("id") }, "node/" + p.optString("id"), alerts)
    }

    private suspend fun pollChess(api: DeviceApi) {
        val reply = api.request("GET", "/api/chess", null)
        if (reply.status != 200) return
        val games = JSONObject(reply.body).optJSONArray("games") ?: return
        // One notification per game, replaced by its next news; a tap opens the board.
        val alerts = Alerts(Prefs(this).alerts)
        chessGames?.takeIf { alerts.chess }?.let { before ->
            for (n in ChessNews.between(before, games))
                notify(ID_CHESS + (n.game.toIntOrNull(16) ?: 0), "Шахматы · ${n.title}", n.text, "board/${n.game}", alerts)
        }
        chessGames = games
    }

    private fun notify(id: Int, title: String, text: String, route: String, alerts: Alerts = Alerts(Prefs(this).alerts)) {
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) != android.content.pm.PackageManager.PERMISSION_GRANTED) return
        val open = PendingIntent.getActivity(this, id, Intent(this, MainActivity::class.java).putExtra(MainActivity.EXTRA_ROUTE, route)
            .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        notifications.notify(id, Notification.Builder(this, if (alerts.sound && !alerts.quietAt()) CHANNEL_MESSAGES else CHANNEL_QUIET)
            .setSmallIcon(R.drawable.ic_stat).setColor(0xFF1FC2AE.toInt())
            .setContentTitle(title).setContentText(text).setStyle(Notification.BigTextStyle().bigText(text))
            .setContentIntent(open).setAutoCancel(true).build())
    }

    companion object {
        const val ACTION_DISCONNECT = "org.meshmesh.app.DISCONNECT"
        const val CHANNEL_LINK = "link"
        const val CHANNEL_MESSAGES = "messages"
        const val CHANNEL_QUIET = "quiet"
        const val ID_LINK = 1
        const val ID_MESSAGE = 1000 // + a hash of the message: one notification each
        const val ID_CHESS = 100000 // + the game number: one notification per game
        const val ID_LOST = 4
        const val ID_NODE = 200000 // + a hash of the node ID
        val KIND_TEXT = mapOf("wifi" to "Wi-Fi", "ble" to "Bluetooth", "usb" to "USB", "tcp" to "USB через компьютер")
    }
}
