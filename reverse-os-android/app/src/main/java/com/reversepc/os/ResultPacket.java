package com.reversepc.os;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public final class ResultPacket {
    public final String output;
    public final byte[] state;
    private ResultPacket(String output,byte[] state){this.output=output;this.state=state;}
    public static byte[] encode(String output,byte[] state){
        if(output==null||state==null)throw new IllegalArgumentException("Missing packet fields");
        byte[] text=output.getBytes(StandardCharsets.UTF_8);
        if(text.length>16*1024*1024||state.length>32*1024*1024+29)
            throw new IllegalArgumentException("Packet exceeds transport limits");
        ByteBuffer result=ByteBuffer.allocate(13+text.length+state.length).order(ByteOrder.LITTLE_ENDIAN);
        result.put("RXOS1".getBytes(StandardCharsets.US_ASCII)).putInt(text.length).putInt(state.length).put(text).put(state);
        return result.array();
    }
    public static ResultPacket parse(byte[] payload){
        if(payload==null||payload.length<13)
            throw new IllegalArgumentException("Native runtime returned incomplete result");
        for(int i=0;i<5;i++)if(payload[i]!="RXOS1".charAt(i))
            throw new IllegalArgumentException("Native result signature mismatch");
        ByteBuffer header=ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN);
        int textLen=header.getInt(5),stateLen=header.getInt(9);
        if(textLen<0||stateLen<0||textLen>16*1024*1024||
                stateLen>32*1024*1024+29||
                13L+textLen+stateLen!=payload.length)
            throw new IllegalArgumentException("Invalid native result dimensions");
        return new ResultPacket(new String(payload,13,textLen,StandardCharsets.UTF_8),
            Arrays.copyOfRange(payload,13+textLen,payload.length));
    }
}
