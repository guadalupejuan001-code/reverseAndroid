"""Authenticate a real UI reply and native RGB bytes; semantic review stays separate."""
import hashlib,re

def parse_ui_reply(raw):
    clean=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',raw).replace(b'\r',b'')
    codes=[int(x) for x in re.findall(rb'^REVERSE_UI_RESPONSE RC_([0-9]+)$',clean,re.M)]
    assert len(codes)<=1,'Repeated UI reply; the gesture must be submitted exactly once'
    return dict(reply_seen=bool(codes),reply_code=codes[0] if codes else None,
                command_succeeded=codes==[0],command_failed=bool(codes and codes[0]!=0))

def native_pixels(raw,frame):
    assert hashlib.sha256(raw).hexdigest()==frame['ppm_sha256']
    header=raw.split(b'\n',3)
    assert len(header)==4 and header[0]==b'P6' and header[2]==b'255'
    dimensions=header[1].split();assert len(dimensions)==2
    width,height=map(int,dimensions);assert (width,height)==(frame['width'],frame['height'])
    assert 0<width<=16384 and 0<height<=16384 and len(header[3])==width*height*3
    assert hashlib.sha256(header[3]).hexdigest()==frame['rgb_sha256']
    return header[3]

def frame_difference(before_raw,before_frame,after_raw,after_frame):
    before=native_pixels(before_raw,before_frame);after=native_pixels(after_raw,after_frame)
    same=(before_frame['width'],before_frame['height'])==(after_frame['width'],after_frame['height'])
    changed=sum(before[i:i+3]!=after[i:i+3] for i in range(0,len(before),3)) if same else None
    return dict(before_ppm_SHA256=before_frame['ppm_sha256'],after_ppm_SHA256=after_frame['ppm_sha256'],
                before_RGB_SHA256=before_frame['rgb_sha256'],after_RGB_SHA256=after_frame['rgb_sha256'],
                same_dimensions=same,changed_pixels=changed,authentic_frame_changed=not same or changed>0,
                meaningful_UI_response_manually_verified=False)
