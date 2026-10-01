package org.meshmesh.app

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiNetworkSpecifier
import android.os.PatternMatcher
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.withTimeout
import java.io.IOException
import java.net.Inet4Address
import java.net.InetAddress

/**
 * The device's access point (MM-XXXXXX, WPA2, 192.168.4.1, no internet). The app joins it for
 * itself only: the phone keeps mobile data for everything else, including map preparation.
 */
object WifiJoin {
    const val DEVICE_ADDRESS = "192.168.4.1"

    class Joined(private val cm: ConnectivityManager, val network: Network) {
        var onLost: () -> Unit = {}
        internal var callback: ConnectivityManager.NetworkCallback? = null
        fun release() { callback?.let { runCatching { cm.unregisterNetworkCallback(it) } }; callback = null }
    }

    suspend fun join(context: Context, ssid: String?, password: String): Joined {
        val cm = context.getSystemService(ConnectivityManager::class.java)
        val specifier = WifiNetworkSpecifier.Builder().apply {
            if (ssid != null) setSsid(ssid) else setSsidPattern(PatternMatcher("MM-", PatternMatcher.PATTERN_PREFIX))
            setWpa2Passphrase(password)
        }.build()
        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .setNetworkSpecifier(specifier)
            .build()
        val available = CompletableDeferred<Network>()
        var joined: Joined? = null
        val callback = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) { available.complete(network) }
            override fun onUnavailable() { available.completeExceptionally(IOException("Не удалось подключиться к точке доступа: проверьте пароль и что Wi-Fi включён на устройстве")) }
            override fun onLost(network: Network) { joined?.onLost?.invoke() }
        }
        cm.requestNetwork(request, callback)
        val network = try {
            withTimeout(90_000) { available.await() }
        } catch (e: Throwable) {
            runCatching { cm.unregisterNetworkCallback(callback) }
            throw if (e is IOException) e else IOException("Точка доступа не выбрана")
        }
        return Joined(cm, network).also { it.callback = callback; joined = it }
    }

    /** A Wi-Fi network this phone is already on whose subnet holds [host]. */
    fun findNetworkFor(context: Context, host: String): Network? {
        val target = runCatching { InetAddress.getByName(host) as? Inet4Address }.getOrNull() ?: return null
        val cm = context.getSystemService(ConnectivityManager::class.java)
        @Suppress("DEPRECATION")
        for (network in cm.allNetworks) {
            val caps = cm.getNetworkCapabilities(network) ?: continue
            if (!caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) continue
            val props = cm.getLinkProperties(network) ?: continue
            for (la in props.linkAddresses) {
                val a = la.address as? Inet4Address ?: continue
                if (sameSubnet(a.address, target.address, la.prefixLength)) return network
            }
        }
        return null
    }

    private fun sameSubnet(a: ByteArray, b: ByteArray, prefix: Int): Boolean {
        for (i in 0 until 4) {
            val bits = (prefix - i * 8).coerceIn(0, 8)
            val mask = (0xFF shl (8 - bits)) and 0xFF
            if ((a[i].toInt() and mask) != (b[i].toInt() and mask)) return false
        }
        return true
    }
}
