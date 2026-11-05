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

const float PI = 3.14159265359;
float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
float noise2(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1,0)), f.x),
               mix(hash21(i + vec2(0,1)), hash21(i + 1.0), f.x), f.y);
}
mat2 rotation(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c,-s,s,c);
}
vec3 palette(float t) {
    return vec3(0.50) + 0.48*cos(6.2831853*(vec3(0.01,0.35,0.67)+t));
}
float starfield(vec2 uv, float density, float time) {
    vec2 g = uv*density;
    vec2 cell = floor(g), f = fract(g)-0.5;
    float seed = hash21(cell);
    vec2 center = vec2(hash21(cell+11.3),hash21(cell+29.7))-.5;
    float d = length(f-center*.68);
    float bright = smoothstep(.038,.006,d);
    float haze = exp(-d*27.0)*.32;
    float twinkle = .72+.28*sin(time*(1.0+seed*3.0)+seed*30.0);
    return (bright+haze)*twinkle*step(.865,seed);
}
vec3 portal(vec2 p,float t) {
    float r = length(p);
    float a = atan(p.y,p.x);
    float spin = uWarp*.60/(r+.30) + t*.22;
    vec2 q = rotation(spin)*p;
    float qa = atan(q.y,q.x);
    float tube = 15.0*log(r+.065) - t*(2.6+uSpeed*.7);
    float rings = pow(max(0.0,1.0-abs(sin(tube))),9.0);
    float spirals = pow(max(0.0,1.0-abs(sin(qa*8.0+11.0*log(r+.13)-t*1.5))),15.0);
    float veins = pow(max(0.0,1.0-abs(sin(qa*16.0+t*.8))),32.0);
    float envelope = smoothstep(.09,.30,r)*(1.0-smoothstep(1.5,2.4,r));
    float nebula = noise2(q*5.0+vec2(t*.13,-t*.12));
    vec3 color = palette(qa/6.2831 + .08*t + r*.20);
    color *= (.07 + rings*1.7 + spirals*.8 + veins*.11)*envelope;
    color += vec3(.06,.025,.15)*nebula*envelope;
    float stars = starfield(q*3.8 + vec2(t*.07,t*.09),17.0,t);
    stars += starfield(q*1.9-vec2(t*.03),32.0,t)*.45;
    color += vec3(.53,.8,1.0)*stars*envelope*.65;
    float edge = exp(-pow((r-(.19+.025*sin(t)))*35.0,2.0));
    color += vec3(.08,.75,1.0)*edge*1.65;
    color += vec3(1.0,.1,.8)*edge*.5;
    color *= smoothstep(.035,.14,r);
    return color;
}
vec3 fracture(vec2 p,float t) {
    vec2 q = p;
    float band = floor((q.y+.8)*55.0);
    float jump = hash21(vec2(band,floor(t*8.0)));
    q.x += (jump-.5)*uGlitch*.27*step(.68,jump);
    vec2 tiled = q*vec2(12.0,10.0);
    vec2 f=fract(tiled)-.5;
    vec2 id=floor(tiled);
    float n=hash21(id);
    float slash=abs(f.x*.76+f.y*.38-.12*sin(n*6.0+t));
    float hair=1.0-smoothstep(.005,.022,slash);
    float tiles=pow(1.0-min(.99,min(abs(f.x),abs(f.y))*2.0),15.0);
    float fan=pow(max(0.0,1.0-abs(sin(atan(q.y,q.x)*12.0+t*.5))),40.0);
    float seam=abs(sin(16.0*q.x+4.0*sin(q.y*7.0+t))*
                   cos(11.0*q.y-2.0*sin(q.x*6.0-t)));
    float fractureLine = 1.0-smoothstep(.025,.11,seam);
    vec3 color=palette(n*.28+q.x*.11+t*.027);
    color*=.14+tiles*.12+hair*(.4+uWarp*.55)+fractureLine*.65+fan*.22;
    color+=vec3(.12,.05,.25)*noise2(q*6.0+vec2(t*.2));
    color+=vec3(.3,.86,1.0)*starfield(q,26.0,t)*1.1;
    color*=.35+uGlitch*.6;
    return color;
}
vec3 ether(vec2 p,float t) {
    vec2 q=p;
    float r=length(q);
    float wave1=sin(q.x*19.0+4.0*sin(q.y*5.0+t*.55)-t*2.0);
    float wave2=sin(q.y*17.0+4.0*sin(q.x*4.0-t*.65)+t*2.2);
    float pattern=exp(-abs(wave1)*18.0)+exp(-abs(wave2)*18.0);
    float interference=sin(q.x*23.0+q.y*11.0-t*2.0)*sin(q.y*16.0-q.x*9.0+t*1.6);
    vec2 flow=q+vec2(sin(q.y*4.0+t)*.30,cos(q.x*4.0-t)*.30)*uWarp*.3;
    float pulse=exp(-pow((r-.43-.12*sin(t*.62))*11.0,2.0));
    vec3 color=palette(interference*.09+flow.y*.2+t*.035);
    color*=(.04+pattern*.65+pulse*.55);
    color+=vec3(.08,.35,.65)*exp(-r*2.5)*(noise2(flow*8.0+t*.1)*.45);
    color+=vec3(.78,.65,1.0)*starfield(flow*1.6,26.0,t);
    return color;
}
vec3 mirrorworld(vec2 p,float t) {
    float r=length(p);
    float angle=atan(p.y,p.x)+t*.065;
    const float wedge=PI/6.0;
    float folded=abs(mod(angle+wedge,2.0*wedge)-wedge);
    vec2 q=vec2(cos(folded),sin(folded))*r;
    q=rotation(t*.12)*q;
    float edge=pow(max(0.0,1.0-abs(sin(21.0*q.x+17.0*q.y+t*.55))),13.0);
    float contour=pow(max(0.0,1.0-abs(sin(24.0*length(q)-t*2.0))),14.0);
    float facets=pow(max(0.0,1.0-abs(sin(atan(q.y,q.x)*12.0))),15.0);
    vec3 color=palette(q.x*.20+q.y*.25+t*.028);
    color*=.10+edge*.75+contour*.85+facets*.4;
    color+=vec3(.04,.18,.4)*exp(-r*3.0);
    color+=vec3(.65,.8,1.0)*starfield(q*2.0,30.0,t)*.4;
    return color;
}
vec3 scene(vec2 p,float t) {
    if(uMode<.5) return portal(p,t);
    if(uMode<1.5) return fracture(p,t);
    if(uMode<2.5) return ether(p,t);
    return mirrorworld(p,t);
}
void main() {
    vec2 uv=vUv;
    float aspect=uResolution.x/max(uResolution.y,1.0);
    vec2 p=(uv-.5)*vec2(aspect,1.0)*2.0/uZoom;
    p -= uCamera*.3;

    float t=uTime*(.30+uSpeed*.70);
    float r=length(p);
    float wave=sin(14.0*r-t*3.0);
    p+=normalize(p+vec2(1e-5))*.045*uWarp*wave;
    float activeLine=step(.80,hash21(vec2(floor(uv.y*110.0),floor(t*9.0))));
    p.x+=(hash21(vec2(floor(uv.y*110.0),floor(t*5.0)))-.5)*uGlitch*.24*activeLine;

    // Tap: expanding optical shockwave, localized at real touch position.
    vec2 touch=(uTouch-.5)*vec2(aspect,1.0)*2.0/uZoom;
    float delta=length(p-touch);
    float front=(1.0-uPulse)*1.6;
    float pulseRing=exp(-pow((delta-front)*40.0,2.0))*uPulse;
    p+=normalize(p-touch+vec2(.0001))*pulseRing*.10*uWarp;

    vec3 color=scene(p,t);
    // RGB separation restricted to the fracture mode to reduce GPU cost.
    if(uMode>.5 && uMode<1.5 && uGlitch>.03) {
        float chroma=uGlitch*(.004+.009*activeLine);
        vec3 left=scene(p+vec2(chroma,0.0),t);
        vec3 right=scene(p-vec2(chroma,0.0),t);
        color.r=left.r; color.b=right.b;
    }
    color+=vec3(.38,.9,1.0)*pulseRing*1.4;
    float scan=.98+.02*sin(uv.y*uResolution.y*.9);
    float snow=(hash21(floor(uv*uResolution.xy*.55)+floor(t*60.0))-.5)
                 *uGlitch*.065;
    color=color*scan+vec3(snow,snow*.8,snow*1.3);
    float vignette=1.0-.55*pow(length((uv-.5)*1.5),2.0);
    color*=max(.25,vignette);
    color=1.0-exp(-max(color,0.0)*1.65);
    color=pow(color,vec3(.83));
    fragColor=vec4(clamp(color,0.0,1.0),1.0);
}
