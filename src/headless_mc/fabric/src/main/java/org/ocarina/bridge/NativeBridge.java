package org.ocarina.bridge;
import java.nio.ByteBuffer;
public final class NativeBridge {
    static { System.load(System.getProperty("steve.native")); }
    private NativeBridge() {}
    public static native boolean open(String session);
    public static native ByteBuffer peek();
    public static native void consume();
    public static native ByteBuffer reserve();
    public static native void publish();
    public static native long now();
    public static native boolean alive();
    public static native void close();
}
