package org.meshmesh.app.flash

import org.junit.Assert.assertEquals
import org.junit.Test
import java.security.MessageDigest

/** The app's DFU packets must be byte for byte those of site/nrf52dfu.js, which installs GAT562 boards from the site. */
class NrfDfuTest {
    @Test fun packetsMatchTheSiteInstaller() {
        // node: dfuPackets(image, dat) of site/nrf52dfu.js for the same input; 6 packets, 1208 bytes.
        val image = ByteArray(1100) { ((it * 37 + 0xC0) and 0xff).toByte() }
        val init = byteArrayOf(1, 2, 0xdb.toByte(), 0xc0.toByte(), 5, 6)
        val packets = NrfDfu.packets(image, init)
        val all = packets.fold(ByteArray(0)) { a, p -> a + p.data }
        assertEquals(6, packets.size)
        assertEquals(1208, all.size)
        assertEquals(0xc7dd, NrfDfu.crc16(image))
        assertEquals("0d4899af853b3072b5f712919aa7fc0fa2e0e922592cc022603f3242ef194016",
            MessageDigest.getInstance("SHA-256").digest(all).joinToString("") { "%02x".format(it) })
    }
}
