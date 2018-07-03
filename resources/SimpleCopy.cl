// Copy src buffer data into dst
__kernel  __attribute__((reqd_work_group_size(64, 1, 1)))
void CopyBuffer(__global unsigned* src, __global unsigned* dst) {
	unsigned tid = get_global_id(0);
	dst[tid] = src[tid];
}