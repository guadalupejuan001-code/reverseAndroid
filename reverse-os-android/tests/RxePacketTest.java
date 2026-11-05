package com.reversepc.os;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
public final class RxePacketTest {
    static int checks;
    static void check(boolean value){if(!value)throw new AssertionError("packet "+checks);checks++;}
    static void reject(Runnable action){try{action.run();throw new AssertionError("accepted invalid packet");}catch(IllegalArgumentException expected){checks++;}}
    public static void main(String[] args){
        byte[] state=new byte[209659];for(int i=0;i<state.length;i++)state[i]=(byte)i;
        String metadata="{\"text\":\"Español · memoria 💾\"}";byte[] encoded=ResultPacket.encode(metadata,state);ResultPacket decoded=ResultPacket.parse(encoded);
        check(metadata.equals(decoded.output));check(Arrays.equals(state,decoded.state));check(encoded.length==13+metadata.getBytes(java.nio.charset.StandardCharsets.UTF_8).length+state.length);
        for(int n=0;n<13;n++){final byte[] cut=Arrays.copyOf(encoded,n);reject(()->ResultPacket.parse(cut));}
        for(int at:new int[]{0,4,5,9}){byte[] bad=encoded.clone();bad[at]^=0x40;reject(()->ResultPacket.parse(bad));}
        byte[] bad=encoded.clone();ByteBuffer.wrap(bad).order(ByteOrder.LITTLE_ENDIAN).putInt(9,32*1024*1024+30);reject(()->ResultPacket.parse(bad));
        reject(()->ResultPacket.encode(null,state));reject(()->ResultPacket.encode(metadata,null));
        check(Arrays.equals(encoded,ResultPacket.encode(decoded.output,decoded.state)));
        System.out.println("RXE_PACKET_PASS checks="+checks);
    }
}
