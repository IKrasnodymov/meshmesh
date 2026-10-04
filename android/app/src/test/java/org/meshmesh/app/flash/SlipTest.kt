package org.meshmesh.app.flash

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertNull
import org.junit.Test

/** ROM loader answers arrive in pieces, after boot text, with escaped 0xC0 and 0xDB inside. */
class SlipTest {
    private fun bytes(vararg v: Int) = ByteArray(v.size) { v[it].toByte() }

    @Test fun escapedFrameSplitAcrossReads() {
        val packet = bytes(1, 0x13, 0xC0, 0xDB, 0x00, 0xDB, 0xDC)
        val wire = "rst:0x15 boot:0x23\r\n".toByteArray() + Slip.encode(packet)
        var buffer = ByteArray(0)
        var frame: ByteArray? = null
        for (i in wire.indices) { // one byte per read
            buffer += wire[i]
            val (f, rest) = Slip.next(buffer)
            buffer = rest
            if (f != null) { frame = f; break }
        }
        assertArrayEquals(packet, frame)
    }

    @Test fun twoFramesInOneRead() {
        val a = bytes(1, 8, 0, 0); val b = bytes(1, 10, 4, 0)
        val (first, rest) = Slip.next(Slip.encode(a) + Slip.encode(b))
        assertArrayEquals(a, first)
        val (second, tail) = Slip.next(rest)
        assertArrayEquals(b, second)
        assertNull(Slip.next(tail).first)
    }
}
