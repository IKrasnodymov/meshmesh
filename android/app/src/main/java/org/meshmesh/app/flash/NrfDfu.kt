package org.meshmesh.app.flash

import java.io.IOException

/**
 * Firmware of nRF52 boards (GAT562): the Nordic legacy serial DFU of the Adafruit nRF52 bootloader,
 * as adafruit-nrfutil ("dfu serial --singlebank") and site/nrf52dfu.js speak it. SLIP-framed HCI
 * packets with CRC16, each acknowledged. Only the application is written: MeshMesh storage and the
 * InternalFS stay. Blocking; run it off the main thread.
 */
object NrfDfu {
    private const val HCI_TYPE = 14; private const val START = 3; private const val INIT = 1
    private const val DATA = 4; private const val STOP = 5; private const val MODE_APP = 4
    const val CHUNK = 512
    private const val PAGE = 4096
    private const val PAGE_ERASE_MS = 89.7; private const val PAGE_WRITE_MS = PAGE / 4 * 0.1 // as nrfutil

    fun crc16(bytes: ByteArray, from: Int = 0, to: Int = bytes.size, start: Int = 0xffff): Int {
        var crc = start
        for (i in from until to) {
            crc = ((crc shr 8) and 0xff) or ((crc shl 8) and 0xff00)
            crc = crc xor (bytes[i].toInt() and 0xff)
            crc = crc xor ((crc and 0xff) shr 4)
            crc = crc xor ((crc shl 12) and 0xffff)
            crc = crc xor (((crc and 0xff) shl 5) and 0xffff)
        }
        return crc and 0xffff
    }

    private fun u32(v: Int) = byteArrayOf(v.toByte(), (v shr 8).toByte(), (v shr 16).toByte(), (v shr 24).toByte())

    /** One HCI packet for sequence number [seq] (1..7, then 0): header, payload, CRC16, SLIP escapes. */
    fun hciPacket(seq: Int, payload: ByteArray): ByteArray {
        val n = payload.size
        val h0 = seq or (((seq + 1) % 8) shl 3) or (1 shl 6) or (1 shl 7)
        val h1 = HCI_TYPE or ((n and 0x0f) shl 4)
        val h2 = (n and 0xff0) shr 4
        val h3 = (-(h0 + h1 + h2)) and 0xff
        val body = byteArrayOf(h0.toByte(), h1.toByte(), h2.toByte(), h3.toByte()) + payload
        val crc = crc16(body)
        return Slip.encode(body + byteArrayOf(crc.toByte(), (crc shr 8).toByte()))
    }

    /** The init packet (firmware.dat) ends with the CRC16 of the image it describes. */
    fun initMatches(image: ByteArray, init: ByteArray) =
        init.size > 2 && crc16(image) == ((init[init.size - 2].toInt() and 0xff) or ((init[init.size - 1].toInt() and 0xff) shl 8))

    class Packet(val kind: Char, val data: ByteArray) // 's'tart, 'i'nit, 'd'ata, 'e'nd

    /** All packets of an application update, in order, numbered as nrfutil numbers them. */
    fun packets(image: ByteArray, init: ByteArray): List<Packet> {
        var seq = 0
        fun next(kind: Char, payload: ByteArray): Packet { seq = (seq + 1) % 8; return Packet(kind, hciPacket(seq, payload)) }
        val list = arrayListOf(next('s', u32(START) + u32(MODE_APP) + u32(0) + u32(0) + u32(image.size)),
            next('i', u32(INIT) + init + byteArrayOf(0, 0)))
        for (at in image.indices step CHUNK) list += next('d', u32(DATA) + image.copyOfRange(at, minOf(image.size, at + CHUNK)))
        list += next('e', u32(STOP))
        return list
    }

    /** Writes [image] described by [init]; [progress] gets 0..100. The bootloader starts the new application. */
    fun flash(io: SerialIo, image: ByteArray, init: ByteArray, progress: (String, Int) -> Unit) {
        val all = packets(image, init)
        val chunks = all.count { it.kind == 'd' }
        var sent = 0
        io.flushInput()
        for (p in all) {
            send(io, p.data)
            when (p.kind) {
                's' -> { // the application area is erased first
                    progress("Стирание памяти платы…", 0)
                    Thread.sleep(maxOf(500.0, (image.size / PAGE + 1) * PAGE_ERASE_MS).toLong())
                }
                'd' -> {
                    if (++sent % 8 == 1) Thread.sleep(PAGE_WRITE_MS.toLong().coerceAtLeast(1)) // a 4 KB page every 8 packets
                    progress("Запись прошивки…", sent * 100 / chunks)
                }
                'e' -> Thread.sleep((PAGE_ERASE_MS + PAGE_WRITE_MS).toLong()) // the bootloader settings page
            }
        }
    }

    /** Writes one packet and waits for the bootloader's acknowledgement (a SLIP frame back). */
    private fun send(io: SerialIo, packet: ByteArray) {
        io.write(packet)
        var marks = 0
        val end = System.currentTimeMillis() + 2000
        while (System.currentTimeMillis() < end) {
            for (b in io.read(100)) if (b == 0xC0.toByte()) marks++
            if (marks >= 2) return
        }
        throw IOException("Загрузчик платы не подтвердил пакет")
    }
}
