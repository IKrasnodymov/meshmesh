package org.meshmesh.app.link

import java.io.ByteArrayOutputStream

/** Splits a byte stream into UTF-8 lines; a multi-byte character may arrive split across chunks. */
class LineAssembler(private val maxLine: Int = 4 * 1024 * 1024) {
    private val pending = ByteArrayOutputStream()

    fun feed(bytes: ByteArray, count: Int = bytes.size): List<String> {
        val lines = ArrayList<String>(1)
        for (i in 0 until count) {
            val b = bytes[i]
            if (b == '\n'.code.toByte()) {
                lines += pending.toString(Charsets.UTF_8.name()).trimEnd('\r')
                pending.reset()
            } else if (pending.size() < maxLine) {
                pending.write(b.toInt())
            }
        }
        return lines
    }

    fun reset() = pending.reset()
}

/**
 * Which line answers a command. The firmware answers each command with exactly one line
 * (src/main.cpp usbLine, src/Portal.cpp BLE notifications); USB may also carry boot and driver
 * log lines, which never start like an answer. Same rules as tools/device.py.
 */
object Replies {
    private val jsonCommands = setOf(
        "status", "config", "key", "messages", "nodes", "ui", "navigation", "connections", "clock",
        "bleprobe", "wifiprobe", "map info", "map areas", "radar", "radar web", "internet", "internet info",
        "chess", "chess web", "channels",
    )

    fun expectsJson(command: String) = command in jsonCommands || command.startsWith("chess show ")

    /** True when [line] is the answer to [command]. */
    fun answers(command: String, line: String): Boolean {
        if (isError(line)) return true
        if (expectsJson(command)) return looksLikeJson(line)
        // "channel do" answers OK…, or JSON for the action "probe".
        return line.startsWith("OK") || (command.startsWith("channel do ") && looksLikeJson(line))
    }

    /** True for any line that is some command's answer (used to drop late answers). */
    fun anyAnswer(line: String) = line.startsWith("OK") || isError(line) || looksLikeJson(line)

    fun isError(line: String) = line.startsWith("ERR") || line.startsWith("Commands:")

    private fun looksLikeJson(line: String): Boolean {
        val t = line.trimEnd()
        return (t.startsWith("{") && t.endsWith("}")) || (t.startsWith("[") && t.endsWith("]"))
    }
}
