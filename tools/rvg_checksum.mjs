import {fnv1a64Wasm} from './rvg_checksum_wasm.mjs';
// Exact FNV-1a-64, using two uint32 limbs. No guest state is modified.
// Prime = 2^40 + 435. The largest Number product is < 2^41, so it is exact.
export function fnv1a64(bytes, length=bytes.length) {
  if (!(bytes instanceof Uint8Array) || !Number.isSafeInteger(length) || length<0 || length>bytes.length)
    throw new TypeError('FNV requires a byte array and an in-bounds exact length');
  if(process.env.REVERSE_PC_FAST_FNV==='1')return fnv1a64Wasm(bytes,length);
  let lo=0x84222325, hi=0xcbf29ce4;
  for(let i=0;i<length;i++) {
    const x=(lo^bytes[i])>>>0;
    const product=x*435;
    hi=(Math.imul(hi,435)+Math.imul(x,256)+Math.floor(product/4294967296))>>>0;
    lo=product>>>0;
  }
  return (BigInt(hi)<<32n)|BigInt(lo);
}
