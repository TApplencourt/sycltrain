#include <stdio.h>
#include <CL/sycl.hpp>
#include <iostream>

#define WORKSIZE 256
#define WORKITEM 64

using namespace cl;

int main (int argc, char **argv)
{
    const sycl::device_selector &gpu_dev = sycl::gpu_selector();
    sycl::queue q(gpu_dev);

    q.submit([&](sycl::handler &cgh)
        {
            auto acc = sycl::accessor<int, 1,  sycl::access::mode::read_write, sycl::access::target::local>(sycl::range<1>(WORKSIZE), cgh);

            cgh.parallel_for<class kernel1>(sycl::nd_range<1>(sycl::range<1>(WORKSIZE), sycl::range<1>(WORKITEM)),
                [=](sycl::nd_item<1> i)
                {
                    int x = i.get_global_linear_id();
                    int y = i.get_local_linear_id();
                    acc[y] = x;
                    i.barrier();
                    if (acc[y] != x)
                    {
                        printf("unexpected value: %d %d\n", acc[y], x);
                    }
                }
            );
        }
    );
    q.wait();

    return 0;
}
