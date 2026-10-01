package org.meshmesh.app

import android.Manifest
import android.annotation.SuppressLint
import android.app.PendingIntent
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.graphics.Color
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.net.Uri
import android.net.wifi.WifiManager
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.os.ParcelUuid
import android.view.ViewGroup
import android.webkit.ValueCallback
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.FrameLayout
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import androidx.webkit.WebViewAssetLoader
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.json.JSONArray
import org.json.JSONObject
import org.meshmesh.app.link.BleLink
import org.meshmesh.app.link.UsbLink
import java.io.ByteArrayInputStream
import java.util.Base64

/**
 * The device's web interface (web/index.html) in a WebView, with host.js: a connection screen
 * for Wi-Fi, Bluetooth and USB, and fetch('/api/…') answered by [MeshService] over that link.
 */
class MainActivity : ComponentActivity() {
    private lateinit var web: WebView
    private var service: MeshService? = null
    private var pageReady = false
    private var pendingIntent: Intent? = null
    private val bluetooth by lazy { getSystemService(BluetoothManager::class.java)?.adapter }
    private val usb by lazy { getSystemService(UsbManager::class.java) }

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            val s = (binder as MeshService.Local).service
            service = s
            s.uiVisible = lifecycle.currentState.isAtLeast(androidx.lifecycle.Lifecycle.State.RESUMED)
            s.onState = { json -> js("MeshHost.state($json)") }
            if (pageReady) js("MeshHost.state(${s.stateJson()})")
            pendingIntent?.let { pendingIntent = null; handleIntent(it) }
        }
        override fun onServiceDisconnected(name: ComponentName) { service = null }
    }

    // Permissions, Android pickers and file dialogs: one pending continuation each.
    private var afterPermissions: ((Boolean) -> Unit)? = null
    private val permissions = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { r ->
        afterPermissions?.let { afterPermissions = null; it(r.values.all { v -> v }) }
    }
    private var afterBluetoothOn: (() -> Unit)? = null
    private val enableBluetooth = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        if (bluetooth?.isEnabled == true) afterBluetoothOn?.invoke() else toast("Bluetooth выключен", "warn")
        afterBluetoothOn = null
    }
    private var fileCallback: ValueCallback<Array<Uri>>? = null
    private val openFile = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        fileCallback?.onReceiveValue(uri?.let { arrayOf(it) }); fileCallback = null
    }
    private var saving: ByteArray? = null
    private var saveType = "application/octet-stream"
    private val saveFile = registerForActivityResult(object : ActivityResultContracts.CreateDocument("*/*") {
        override fun createIntent(context: Context, input: String) = super.createIntent(context, input).setType(saveType)
    }) { uri ->
        val data = saving; saving = null
        if (uri == null || data == null) return@registerForActivityResult
        runCatching { contentResolver.openOutputStream(uri)!!.use { it.write(data) } }
            .onSuccess { toast("Файл сохранён", "ok") }.onFailure { toast("Файл не сохранён: ${it.message}", "bad") }
    }

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val root = FrameLayout(this).apply { setBackgroundColor(Color.BLACK) }
        web = WebView(this).apply { setBackgroundColor(Color.parseColor("#080C11")) }
        root.addView(web, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        setContentView(root)
        ViewCompat.setOnApplyWindowInsetsListener(root) { v, insets ->
            val b = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.ime() or WindowInsetsCompat.Type.displayCutout())
            v.setPadding(b.left, b.top, b.right, b.bottom)
            WindowInsetsCompat.CONSUMED
        }
        WebView.setWebContentsDebuggingEnabled(BuildConfig.DEBUG)
        web.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true
            mediaPlaybackRequiresUserGesture = false
            allowFileAccess = false
            allowContentAccess = true
        }
        val loader = WebViewAssetLoader.Builder().addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(this)).build()
        web.webViewClient = object : WebViewClient() {
            override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse? {
                val url = request.url
                if (url.host != HOST) return null
                if (url.path == PAGE) return page()
                return loader.shouldInterceptRequest(url)
            }
            override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean {
                if (request.url.host == HOST) return false
                runCatching { startActivity(Intent(Intent.ACTION_VIEW, request.url)) }
                return true
            }
            override fun onPageFinished(view: WebView, url: String) {
                pageReady = true
                service?.let { js("MeshHost.state(${it.stateJson()})") }
            }
        }
        web.webChromeClient = object : WebChromeClient() {
            override fun onShowFileChooser(view: WebView, callback: ValueCallback<Array<Uri>>, params: FileChooserParams): Boolean {
                fileCallback?.onReceiveValue(null)
                fileCallback = callback
                openFile.launch(arrayOf("*/*"))
                return true
            }
        }
        web.addJavascriptInterface(Bridge(), "MeshNative")
        web.loadUrl("https://$HOST$PAGE")
        bindService(Intent(this, MeshService::class.java), connection, BIND_AUTO_CREATE)
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                web.evaluateJavascript("window.MeshHost?MeshHost.back():false") { handled -> if (handled != "true") moveTaskToBack(true) }
            }
        })
        handleIntent(intent)
    }

    /** web/index.html with host.css and host.js added. */
    private fun page(): WebResourceResponse {
        val html = assets.open("web/index.html").bufferedReader().readText()
            .replaceFirst("</head>", "<link rel=\"stylesheet\" href=\"/assets/host.css\"></head>")
            .replaceFirst("</body>", "<script src=\"/assets/host.js\"></script></body>")
        return WebResourceResponse("text/html", "utf-8", ByteArrayInputStream(html.toByteArray()))
    }

    override fun onResume() { super.onResume(); service?.uiVisible = true }
    override fun onPause() { super.onPause(); service?.uiVisible = false }

    override fun onDestroy() {
        stopBleScan()
        runCatching { unbindService(connection) }
        web.destroy()
        super.onDestroy()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    private fun handleIntent(intent: Intent?) {
        intent ?: return
        if (service == null) { pendingIntent = intent; return }
        intent.getStringExtra(EXTRA_ROUTE)?.let { route -> js("MeshHost.go(${JSONObject.quote(route)})") }
        if (intent.action == UsbManager.ACTION_USB_DEVICE_ATTACHED) {
            val device: UsbDevice? = if (Build.VERSION.SDK_INT >= 33) intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                else @Suppress("DEPRECATION") intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
            // Plugged in while nothing else is connected: connect to it (Android granted access with this intent).
            if (device != null && service?.api == null) connectUsb(device)
            else js("MeshHost.devices('usb',${usbList()})")
        }
    }

    private fun js(code: String) {
        web.post { if (pageReady || code.startsWith("MeshHost.done")) web.evaluateJavascript("window.MeshHost&&$code", null) }
    }

    private fun toast(text: String, tone: String) = js("MeshHost.toast(${JSONObject.quote(text)},'$tone')")

    private fun need(list: List<String>, then: () -> Unit, denied: String) {
        val missing = list.filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isEmpty()) { then(); return }
        afterPermissions = { ok -> if (ok) then() else toast(denied, "bad") }
        permissions.launch(missing.toTypedArray())
    }

    private fun blePermissions() = if (Build.VERSION.SDK_INT >= 31) listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else listOf(Manifest.permission.ACCESS_FINE_LOCATION)

    private fun wifiPermissions() = if (Build.VERSION.SDK_INT >= 33) listOf(Manifest.permission.NEARBY_WIFI_DEVICES)
        else listOf(Manifest.permission.ACCESS_FINE_LOCATION)

    private fun withNotifications(then: () -> Unit) {
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            afterPermissions = { then() } // a refusal only means no notifications
            permissions.launch(arrayOf(Manifest.permission.POST_NOTIFICATIONS))
        } else then()
    }

    private fun withBluetooth(then: () -> Unit) = need(blePermissions(), {
        val adapter = bluetooth
        when {
            adapter == null -> toast("В этом телефоне нет Bluetooth", "bad")
            adapter.isEnabled -> then()
            else -> { afterBluetoothOn = then; enableBluetooth.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)) }
        }
    }, "Без разрешения Bluetooth устройства не найти")

    // Scans: results go to the page as they come.

    private var bleScan: ScanCallback? = null
    private val bleFound = LinkedHashMap<String, JSONObject>()

    @SuppressLint("MissingPermission")
    private fun startBleScan() = withBluetooth {
        stopBleScan()
        bleFound.clear()
        bluetooth?.bondedDevices?.filter { it.name?.startsWith("MeshMesh ") == true }?.forEach {
            bleFound[it.address] = JSONObject().put("id", it.address).put("name", it.name).put("rssi", JSONObject.NULL).put("bonded", true)
        }
        val scanner = bluetooth?.bluetoothLeScanner ?: return@withBluetooth
        val callback = object : ScanCallback() {
            override fun onScanResult(type: Int, r: ScanResult) {
                val name = r.scanRecord?.deviceName ?: r.device.name ?: "MeshMesh"
                bleFound[r.device.address] = JSONObject().put("id", r.device.address).put("name", name).put("rssi", r.rssi)
                    .put("bonded", r.device.bondState == android.bluetooth.BluetoothDevice.BOND_BONDED)
                pushBle(false)
            }
            override fun onScanFailed(errorCode: Int) { toast("Поиск Bluetooth не запустился (код $errorCode)", "bad"); pushBle(true) }
        }
        bleScan = callback
        scanner.startScan(listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(BleLink.SERVICE)).build()),
            ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), callback)
        pushBle(false)
        lifecycleScope.launch { delay(15_000); if (bleScan === callback) { stopBleScan(); pushBle(true) } }
    }

    private fun pushBle(done: Boolean) = js("MeshHost.devices('ble',${JSONArray(bleFound.values.toList())},$done)")

    @SuppressLint("MissingPermission")
    private fun stopBleScan() {
        val callback = bleScan ?: return
        bleScan = null
        runCatching { bluetooth?.bluetoothLeScanner?.stopScan(callback) }
    }

    private fun usbList(): String = JSONArray(UsbLink.serialDevices(this).map {
        JSONObject().put("id", it.deviceName).put("name", UsbLink.describe(it)).put("native", UsbLink.isNative(it))
    }).toString()

    private var wifiReceiver: BroadcastReceiver? = null

    @SuppressLint("MissingPermission")
    private fun startWifiScan() = need(wifiPermissions(), {
        val wm = applicationContext.getSystemService(WifiManager::class.java)
        if (!wm.isWifiEnabled) { toast("Включите Wi-Fi на телефоне", "warn"); return@need }
        fun push(done: Boolean) {
            val prefs = Prefs(this)
            val list = wm.scanResults.mapNotNull { r ->
                @Suppress("DEPRECATION") val ssid = if (Build.VERSION.SDK_INT >= 33) r.wifiSsid?.toString()?.trim('"') else r.SSID
                if (ssid == null || !ssid.startsWith("MM-")) null
                else JSONObject().put("id", ssid).put("name", ssid).put("rssi", r.level).put("saved", prefs.password(ssid) != null)
            }.distinctBy { it.getString("id") }.sortedByDescending { it.getInt("rssi") }
            js("MeshHost.devices('wifi',${JSONArray(list)},$done)")
        }
        wifiReceiver?.let { runCatching { unregisterReceiver(it) } }
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) { push(true); runCatching { unregisterReceiver(this) }; wifiReceiver = null }
        }
        wifiReceiver = receiver
        registerReceiver(receiver, IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION))
        push(false)
        @Suppress("DEPRECATION") if (!wm.startScan()) { push(true); runCatching { unregisterReceiver(receiver) }; wifiReceiver = null }
    }, "Без разрешения список сетей недоступен: подключитесь через запрос Android")

    // Connecting.

    private fun connectUsb(device: UsbDevice) {
        if (usb.hasPermission(device)) { withNotifications { service?.connectUsb(device) }; return }
        val action = "$packageName.USB_PERMISSION"
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                runCatching { unregisterReceiver(this) }
                if (i.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false) || usb.hasPermission(device)) withNotifications { service?.connectUsb(device) }
                else toast("Доступ к USB не разрешён", "bad")
            }
        }
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(receiver, IntentFilter(action), RECEIVER_NOT_EXPORTED)
        else registerReceiver(receiver, IntentFilter(action))
        val flags = if (Build.VERSION.SDK_INT >= 31) PendingIntent.FLAG_MUTABLE else 0
        usb.requestPermission(device, PendingIntent.getBroadcast(this, 0, Intent(action).setPackage(packageName), flags))
    }

    @SuppressLint("MissingPermission")
    private fun connect(spec: JSONObject) {
        val s = service ?: return toast("Служба ещё запускается, повторите", "warn")
        when (spec.optString("kind")) {
            "usb" -> {
                val id = spec.optString("id")
                val device = UsbLink.serialDevices(this).firstOrNull { id.isEmpty() || it.deviceName == id }
                if (device == null) toast("Плата по USB не найдена: подключите её кабелем (OTG)", "warn") else connectUsb(device)
            }
            "ble" -> withBluetooth {
                stopBleScan()
                val device = runCatching { bluetooth!!.getRemoteDevice(spec.getString("id")) }.getOrNull()
                if (device == null) toast("Неверный адрес Bluetooth", "bad") else withNotifications { s.connectBle(device) }
            }
            "wifi" -> {
                val address = spec.optString("address").ifBlank { WifiJoin.DEVICE_ADDRESS }
                Prefs(this).address = address
                val ssid = spec.optString("ssid").takeIf { it.isNotBlank() }
                withNotifications { s.connectWifi(ssid, spec.optString("password"), address) }
            }
            "tcp" -> {
                val host = spec.optString("host"); val port = spec.optInt("port", 8771)
                Prefs(this).bridge = "$host:$port"
                withNotifications { s.connectTcp(host, port) }
            }
        }
    }

    /** Called by host.js (on a WebView thread). */
    inner class Bridge {
        @android.webkit.JavascriptInterface
        fun request(id: Int, method: String, path: String, body: String?) {
            lifecycleScope.launch {
                val r = service?.request(method, path, body) ?: org.meshmesh.app.api.ApiReply.failed("Служба не запущена")
                js("MeshHost.done($id,${r.status},${JSONObject.quote(r.body)},${r.base64})")
            }
        }
        @android.webkit.JavascriptInterface fun state(): String = service?.stateJson() ?: "{\"state\":\"idle\"}"
        @android.webkit.JavascriptInterface fun prefs(): String = Prefs(this@MainActivity).snapshot().toString()
        @android.webkit.JavascriptInterface fun scan(kind: String) = runOnUiThread {
            when (kind) {
                "ble" -> startBleScan()
                "wifi" -> startWifiScan()
                "usb" -> js("MeshHost.devices('usb',${usbList()},true)")
            }
        }
        @android.webkit.JavascriptInterface fun stopScan() = runOnUiThread { stopBleScan() }
        @android.webkit.JavascriptInterface fun connect(json: String) = runOnUiThread { connect(JSONObject(json)) }
        @android.webkit.JavascriptInterface fun disconnect() = runOnUiThread { service?.disconnect("Отключено") }
        @android.webkit.JavascriptInterface fun saveFile(name: String, type: String, base64: String) = runOnUiThread {
            saving = Base64.getDecoder().decode(base64)
            saveType = type.ifBlank { "application/octet-stream" }
            saveFile.launch(name)
        }
        @android.webkit.JavascriptInterface fun version(): String = BuildConfig.VERSION_NAME
    }

    companion object {
        const val HOST = "appassets.androidplatform.net"
        const val PAGE = "/assets/web/index.html"
        const val EXTRA_ROUTE = "route"
    }
}
