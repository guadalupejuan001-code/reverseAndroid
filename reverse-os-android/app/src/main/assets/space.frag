#version 300 es
precision highp float;
in vec2 vUv;
out vec4 fragColor;
uniform vec2 uResolution;
uniform float uTime;
uniform float uMode;
uniform float uGlitch;
uniform float uWarp;
uniform float uSpeed;
uniform vec2 uCamera;
uniform vec2 uTouch;
uniform float uPulse;
uniform float uZoom;

// Flat, restrained desktop wallpaper. Drawn by the existing OpenGL ES 3
// adaptive renderer. No image assets, bloom, particles-as-spectacle or
// synthetic "quantum computation" effects.
const float TAU=6.28318530718;

float thin(float distance,float width){
    return 1.0-smoothstep(width,width+0.0025,abs(distance));
}
float dotAt(vec2 p,vec2 center,float radius){
    return 1.0-smoothstep(radius,radius+0.0035,length(p-center));
}
float ellipse(vec2 p,vec2 scale){
    return length(p/scale)-1.0;
}
void main(){
    float aspect=uResolution.x/max(1.0,uResolution.y);
    vec2 p=(vUv-.5)*vec2(aspect,1.0);
    vec2 q=p-vec2(0.15,0.012);
    float t=uTime*max(0.0,uSpeed);
    vec3 base=vec3(.074,.092,.117);
    vec3 frame=vec3(.19,.255,.315);
    vec3 ring=vec3(.255,.325,.395);
    vec3 highlight=vec3(.64,.72,.81);

    // Solid central world, no fake texture or dynamic lighting.
    float d=length(q);
    float disc=1.0-smoothstep(.266,.270,d);
    base=mix(base,vec3(.110,.144,.188),disc);
    base=mix(base,frame,thin(d-.267,.0011)*.85);

    // Orbit rules: paths stay fixed while small markers move.
    float a=thin(ellipse(q,vec2(.65,.205)),.0035);
    float b=thin(ellipse(q,vec2(.45,.315)),.0030);
    float c=thin(ellipse(q,vec2(.325,.360)),.0028);
    base=mix(base,ring,clamp((a+b*.55+c*.4)*.42,0.0,.65));

    // Slowly travelling markers indicate actual continuous frames without
    // visual clutter; they are not representations of measured qubits.
    vec2 m1=vec2(.65*cos(t*.34+.2),.205*sin(t*.34+.2));
    vec2 m2=vec2(.45*cos(-t*.26+2.1),.315*sin(-t*.26+2.1));
    vec2 m3=vec2(.65*cos(t*.34+3.15),.205*sin(t*.34+3.15));
    float markers=dotAt(q,m1,.009)+dotAt(q,m2,.0065)+dotAt(q,m3,.005);
    base=mix(base,highlight,clamp(markers*.88,0.0,1.0));

    // Quiet center axis and a few intentional anchor points.
    float axis=thin(q.x,.0007)*(1.0-smoothstep(.18,.43,abs(q.y)));
    base=mix(base,ring,axis*.20);
    float center=dotAt(q,vec2(0.,0.),.005);
    base=mix(base,highlight,center*.60);

    // Feedback only on touch; nothing imitates a particle storm.
    vec2 touch=(uTouch-.5)*vec2(aspect,1.0);
    float wave=thin(length(p-touch)-(1.0-uPulse)*.42,.006)*uPulse;
    base=mix(base,ring,wave*.45);

    fragColor=vec4(base,1.0);
}
