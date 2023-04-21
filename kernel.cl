#pragma OPENCL EXTENSION cl_khr_gl_sharing : enable

__kernel void hello_opencgl(__read_only image2d_t input_image, __global unsigned char *output)
{
    int row = get_global_id(0);
    int col = get_global_id(1);
    int size = get_global_size(0);
    int2 coord = (int2)(row, col);
    float4 v = read_imagef(input_image, coord);
    output[col * size + row] = (char)(v.x * 255);
}
