package com.reversepc.os;

/** Pixel geometry shared by touch, snapping, persistence and host tests. */
public final class WindowGeometry {
    public int x,y,width,height;
    public WindowGeometry(int x,int y,int width,int height){
        this.x=x;this.y=y;this.width=width;this.height=height;
    }
    public WindowGeometry copy(){return new WindowGeometry(x,y,width,height);}
    public void fit(int areaWidth,int areaHeight,int minWidth,int minHeight){
        width=Math.max(Math.min(minWidth,areaWidth),Math.min(width,areaWidth));
        height=Math.max(Math.min(minHeight,areaHeight),Math.min(height,areaHeight));
        x=Math.max(0,Math.min(x,areaWidth-width));
        y=Math.max(0,Math.min(y,areaHeight-height));
    }
    public void snap(int side,int areaWidth,int areaHeight){
        y=0;height=areaHeight;
        if(side==0){x=0;width=areaWidth;}
        else if(side<0){x=0;width=areaWidth/2;}
        else {x=areaWidth/2;width=areaWidth-x;}
    }
}
