package org.meshmesh.app.link

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.delay
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.withTimeoutOrNull
import java.io.IOException
import java.util.UUID

/**
 * The MeshMesh GATT service (src/Portal.cpp): commands are written to RX with a response,
 * answers arrive as TX notifications ending with a newline. Both characteristics need an
 * authenticated bond: Android asks for the PIN shown on the device.
 */
@SuppressLint("MissingPermission")
class BleLink private constructor(private val context: Context, private val device: BluetoothDevice) : ByteLink {
    override var onBytes: (ByteArray, Int) -> Unit = { _, _ -> }
    override var onClosed: (String) -> Unit = {}
    override val maxCommand = 255 // src/Portal.cpp ignores longer writes

    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var tx: BluetoothGattCharacteristic? = null
    private val ops = Mutex()
    @Volatile private var op: CompletableDeferred<Int>? = null
    private val connected = CompletableDeferred<Unit>()
    @Volatile private var discovered = CompletableDeferred<Int>()
    private val mtuDone = CompletableDeferred<Int>()
    @Volatile private var open = true
    var mtu = 23
        private set

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) connected.complete(Unit)
            else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                val reason = if (status == 19) "Bluetooth: устройство разорвало связь (BLE выключен?)" else "Bluetooth: связь потеряна"
                connected.completeExceptionally(IOException("Bluetooth: не удалось подключиться (код $status)"))
                op?.complete(-1)
                if (open) { open = false; runCatching { g.close() }; onClosed(reason) }
            }
        }
        override fun onMtuChanged(g: BluetoothGatt, value: Int, status: Int) { if (status == BluetoothGatt.GATT_SUCCESS) mtu = value; mtuDone.complete(value) }
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) { discovered.complete(status) }
        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) { op?.complete(status) }
        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) { op?.complete(status) }
        @Deprecated("Android 12 and older")
        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) { op?.complete(status) }
        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, status: Int) { op?.complete(status) }
        @Deprecated("Android 12 and older")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            val v = c.value ?: return
            if (c.uuid == TX) onBytes(v, v.size)
        }
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) {
            if (c.uuid == TX) onBytes(value, value.size)
        }
    }

    private suspend fun start(onStage: (String) -> Unit) {
        onStage("Подключение по Bluetooth…")
        val g = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
            ?: throw IOException("Bluetooth недоступен")
        gatt = g
        withTimeout(20_000) { connected.await() }
        g.requestMtu(517)
        withTimeoutOrNull(5_000) { mtuDone.await() }
        g.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
        discover(g)
        var service = g.getService(SERVICE)
        if (service == null) {
            // A phone bonded to the board under its previous firmware (same Bluetooth address, e.g. stock
            // MeshCore on a GAT562) keeps that firmware's services in the Android cache: drop it, ask again.
            onStage("Обновление списка сервисов…")
            runCatching { g.javaClass.getMethod("refresh").invoke(g) }
            delay(1_000)
            discover(g)
            service = g.getService(SERVICE) ?: throw IOException("Это не MeshMesh: нет сервиса MeshMesh BLE")
        }
        rx = service.getCharacteristic(RX); tx = service.getCharacteristic(TX)
        if (rx == null || tx == null) throw IOException("Сервис MeshMesh неполный")
        // A protected read starts pairing; Android shows the PIN entry. Retried until bonded.
        if (device.bondState != BluetoothDevice.BOND_BONDED) onStage("Сопряжение: введите PIN с экрана устройства")
        pair(onStage)
        onStage("Подписка на ответы…")
        g.setCharacteristicNotification(tx, true)
        val cccd = tx!!.getDescriptor(CCCD) ?: throw IOException("Нет дескриптора уведомлений")
        val status = operation(10_000) {
            if (Build.VERSION.SDK_INT >= 33) g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS
            else { @Suppress("DEPRECATION") run { cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE; g.writeDescriptor(cccd) } }
        }
        if (status != BluetoothGatt.GATT_SUCCESS) throw IOException("Bluetooth: подписка не удалась (код $status)")
    }

    private suspend fun discover(g: BluetoothGatt) {
        discovered = CompletableDeferred()
        if (!g.discoverServices()) throw IOException("Bluetooth: поиск сервисов не начался")
        if (withTimeout(15_000) { discovered.await() } != BluetoothGatt.GATT_SUCCESS) throw IOException("Bluetooth: сервисы не найдены")
    }

    private suspend fun pair(onStage: (String) -> Unit) {
        val bonded = CompletableDeferred<Int>()
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                val d: BluetoothDevice? = if (Build.VERSION.SDK_INT >= 33) i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
                    else @Suppress("DEPRECATION") i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                if (d?.address != device.address) return
                val state = i.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, -1)
                if (state == BluetoothDevice.BOND_BONDED || state == BluetoothDevice.BOND_NONE) bonded.complete(state)
            }
        }
        context.registerReceiver(receiver, IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED))
        try {
            val deadline = System.currentTimeMillis() + 120_000
            // Only a bond the phone had before this connection can be stale (the board's flash replaced).
            val wasBonded = device.bondState == BluetoothDevice.BOND_BONDED
            var forgotten = false
            var requested = false
            while (true) {
                val status = operation(30_000) { gatt!!.readCharacteristic(tx) }
                if (status == BluetoothGatt.GATT_SUCCESS) return
                if (System.currentTimeMillis() > deadline) throw IOException("Сопряжение не завершено: введите PIN с экрана устройства")
                if (wasBonded && device.bondState == BluetoothDevice.BOND_BONDED && !forgotten && (status == 5 || status == 15 || status == 137)) {
                    // The phone keeps a bond the board no longer has (the board's flash was replaced): pair again.
                    forgotten = true
                    requested = false
                    onStage("Повторное сопряжение: введите PIN с экрана устройства")
                    runCatching { device.javaClass.getMethod("removeBond").invoke(device) }
                    delay(1500)
                }
                if (device.bondState == BluetoothDevice.BOND_NONE && !requested) { requested = true; device.createBond() }
                withTimeoutOrNull(5_000) { bonded.await() }
                delay(500)
            }
        } finally {
            runCatching { context.unregisterReceiver(receiver) }
        }
    }

    private suspend fun operation(timeoutMs: Long, start: () -> Boolean): Int = ops.withLock {
        if (!open) throw LinkClosedException("Bluetooth отключён")
        val done = CompletableDeferred<Int>()
        op = done
        try {
            if (!start()) return@withLock -2
            withTimeoutOrNull(timeoutMs) { done.await() } ?: -3
        } finally {
            op = null
        }
    }

    override suspend fun write(data: ByteArray) {
        val g = gatt ?: throw LinkClosedException("Bluetooth не подключён")
        val c = rx ?: throw LinkClosedException("Bluetooth не подключён")
        var status = -2
        for (attempt in 0 until 3) { // a write may be refused while the stack is busy for a moment
            status = operation(15_000) {
                if (Build.VERSION.SDK_INT >= 33) g.writeCharacteristic(c, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS
                else @Suppress("DEPRECATION") run { c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT; c.value = data; g.writeCharacteristic(c) }
            }
            if (status != -2) break
            delay(100)
        }
        if (status == -1 || !open) throw LinkClosedException("Bluetooth: связь потеряна")
        if (status != BluetoothGatt.GATT_SUCCESS) throw IOException("Bluetooth: команда не записана (код $status)")
    }

    override fun close() {
        if (!open) return
        open = false
        gatt?.let { runCatching { it.disconnect() }; runCatching { it.close() } }
    }

    companion object {
        val SERVICE: UUID = UUID.fromString("7a9e0001-98bd-4d56-89a8-c4eab4179010")
        val RX: UUID = UUID.fromString("7a9e0002-98bd-4d56-89a8-c4eab4179010")
        val TX: UUID = UUID.fromString("7a9e0003-98bd-4d56-89a8-c4eab4179010")
        val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

        suspend fun open(context: Context, device: BluetoothDevice, onStage: (String) -> Unit): BleLink {
            val link = BleLink(context.applicationContext, device)
            try {
                link.start(onStage)
            } catch (e: Throwable) {
                link.close()
                throw e
            }
            return link
        }
    }
}
