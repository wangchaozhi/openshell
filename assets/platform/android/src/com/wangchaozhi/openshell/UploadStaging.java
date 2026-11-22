package com.wangchaozhi.openshell;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.provider.OpenableColumns;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashSet;
import java.util.Set;

public final class UploadStaging {
    private static final String[] COLUMNS = {
        Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE
    };

    private UploadStaging() {}

    public static String copy(Context context, String url, String destination, boolean folder) {
        try {
            ContentResolver resolver = context.getContentResolver();
            Uri tree = Uri.parse(url);
            if (!folder) {
                try (Cursor cursor = resolver.query(tree, new String[]{OpenableColumns.DISPLAY_NAME},
                                                    null, null, null)) {
                    if (cursor == null || !cursor.moveToFirst()) {
                        return null;
                    }
                    File output = new File(destination, safeName(cursor.getString(0)));
                    copyFile(resolver, tree, output);
                    return output.getAbsolutePath();
                }
            }
            String id = DocumentsContract.getTreeDocumentId(tree);
            Uri root = DocumentsContract.buildDocumentUriUsingTree(tree, id);
            try (Cursor cursor = resolver.query(root, COLUMNS, null, null, null)) {
                if (cursor == null || !cursor.moveToFirst()) {
                    return null;
                }
                File output = new File(destination, safeName(cursor.getString(1)));
                copyDocument(resolver, tree, id, cursor.getString(2), output, new HashSet<>());
                return output.getAbsolutePath();
            }
        } catch (Exception failure) {
            return null;
        }
    }

    private static String safeName(String name) throws IOException {
        if (name == null || name.isEmpty() || name.equals(".") || name.equals("..")
                || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) {
            throw new IOException("Invalid document name");
        }
        return name;
    }

    private static void copyDocument(ContentResolver resolver, Uri tree, String id, String mime,
                                     File output, Set<String> ancestors) throws IOException {
        if (Document.MIME_TYPE_DIR.equals(mime)) {
            if (!ancestors.add(id) || !output.mkdir()) {
                throw new IOException("Cannot create staging directory");
            }
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, id);
            try (Cursor cursor = resolver.query(children, COLUMNS, null, null, null)) {
                if (cursor == null) {
                    throw new IOException("Cannot read directory");
                }
                while (cursor.moveToNext()) {
                    File child = new File(output, safeName(cursor.getString(1)));
                    copyDocument(resolver, tree, cursor.getString(0), cursor.getString(2), child, ancestors);
                }
            }
            ancestors.remove(id);
            return;
        }
        copyFile(resolver, DocumentsContract.buildDocumentUriUsingTree(tree, id), output);
    }

    private static void copyFile(ContentResolver resolver, Uri document, File output) throws IOException {
        if (!output.createNewFile()) {
            throw new IOException("Duplicate staging file");
        }
        try (InputStream input = resolver.openInputStream(document);
             FileOutputStream stream = new FileOutputStream(output)) {
            if (input == null) {
                throw new IOException("Cannot read document");
            }
            byte[] buffer = new byte[65536];
            int count;
            while ((count = input.read(buffer)) != -1) {
                stream.write(buffer, 0, count);
            }
        }
    }
}
