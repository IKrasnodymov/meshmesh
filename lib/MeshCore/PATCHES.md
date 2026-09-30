Local bounds fixes (wire format unchanged):

- Utils::MACThenDecrypt rejects non-integral AES blocks and oversized input before decrypting. A valid channel HMAC does not prove ciphertext length is safe.
- Mesh::onRecvPacket checks the decrypted path and extra-type byte fit inside the authenticated plaintext before passing a return path to callbacks.

All other selected upstream sources retain their original implementation.
