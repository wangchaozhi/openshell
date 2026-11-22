package com.wangchaozhi.openshell;

import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

public final class CredentialCrypto {
    private static final String ALIAS = "OpenShell.credentials.v1";
    private static final int IV_LENGTH = 12;

    private CredentialCrypto() {}

    private static SecretKey key(boolean create) throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        if (store.containsAlias(ALIAS)) {
            return (SecretKey) store.getKey(ALIAS, null);
        }
        if (!create) {
            throw new IllegalStateException("Credential key unavailable");
        }
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(
            ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
            .setKeySize(256)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .build());
        return generator.generateKey();
    }

    public static synchronized String seal(String value, String field) {
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, key(true));
            cipher.updateAAD(field.getBytes(StandardCharsets.UTF_8));
            byte[] iv = cipher.getIV();
            if (iv.length != IV_LENGTH) {
                return null;
            }
            byte[] ciphertext = cipher.doFinal(value.getBytes(StandardCharsets.UTF_8));
            byte[] envelope = Arrays.copyOf(iv, iv.length + ciphertext.length);
            System.arraycopy(ciphertext, 0, envelope, iv.length, ciphertext.length);
            return "v1:" + Base64.encodeToString(envelope, Base64.NO_WRAP);
        } catch (Exception failure) {
            return null;
        }
    }

    public static synchronized String unseal(String value, String field) {
        try {
            byte[] envelope = Base64.decode(value.substring(3), Base64.NO_WRAP);
            if (envelope.length < IV_LENGTH + 16) {
                return null;
            }
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, key(false), new GCMParameterSpec(128, envelope, 0, IV_LENGTH));
            cipher.updateAAD(field.getBytes(StandardCharsets.UTF_8));
            byte[] plaintext = cipher.doFinal(envelope, IV_LENGTH, envelope.length - IV_LENGTH);
            return new String(plaintext, StandardCharsets.UTF_8);
        } catch (Exception failure) {
            return null;
        }
    }
}
