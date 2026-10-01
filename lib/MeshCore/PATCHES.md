Local bounds fixes (wire format unchanged):

- Utils::MACThenDecrypt rejects non-integral AES blocks and oversized input before decrypting. A valid channel HMAC does not prove ciphertext length is safe.
- Mesh::onRecvPacket checks the decrypted path and extra-type byte fit inside the authenticated plaintext before passing a return path to callbacks.

- CommonCLI::handleCommand copies command arguments into its 132-byte work buffer with StrHelper::strncpy instead of strcpy (tempradio, sensor set, set radio, set radio.extra.sf); an admin CLI line can be 160 bytes.

All other selected upstream sources retain their original implementation.
