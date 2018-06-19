// Copy src buffer data into dst
__kernel void CopyBuffer(__global unsigned* src, __global unsigned* dst) {
	unsigned threadId = get_global_id(0);
	dst[threadId] = src[threadId];
}