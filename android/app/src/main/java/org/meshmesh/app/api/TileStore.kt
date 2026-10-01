package org.meshmesh.app.api

import java.io.File

/**
 * Map tiles already read from the board's SD (MMT1 files with their 24-byte header).
 * A cached copy is used only when the board's header (size and CRC) still matches.
 */
class TileStore(private val dir: File, private val limitBytes: Long = 200L * 1024 * 1024) {
    init { dir.mkdirs() }

    private fun file(z: Int, x: Long, y: Long) = File(dir, "$z-$x-$y.mmt")

    @Synchronized fun get(z: Int, x: Long, y: Long): ByteArray? = file(z, x, y).takeIf { it.isFile }?.let {
        runCatching { it.setLastModified(System.currentTimeMillis()); it.readBytes() }.getOrNull()
    }

    @Synchronized fun put(z: Int, x: Long, y: Long, data: ByteArray) {
        val target = file(z, x, y)
        val part = File(dir, target.name + ".part")
        runCatching {
            part.writeBytes(data)
            if (!part.renameTo(target)) part.delete()
        }
        trim()
    }

    private fun trim() {
        val files = dir.listFiles { f -> f.name.endsWith(".mmt") } ?: return
        var total = files.sumOf { it.length() }
        if (total <= limitBytes) return
        for (f in files.sortedBy { it.lastModified() }) {
            if (total <= limitBytes * 8 / 10) break
            total -= f.length()
            f.delete()
        }
    }
}
