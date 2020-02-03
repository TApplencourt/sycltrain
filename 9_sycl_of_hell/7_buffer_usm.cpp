#include <CL/sycl.hpp>

namespace sycl = cl::sycl;

int main(int argc, char** argv) {

   const auto global_range =  (size_t) atoi(argv[1]);
   const auto local_range =  (size_t) atoi(argv[2]);

  // Selectors determine which device kernels will be dispatched to.
  sycl::default_selector selector;
  sycl::queue myQueue(selector);

  sycl::device dev = myQueue.get_device();
  sycl::context ctex = myQueue.get_context();

  int* A = static_cast<int*>(sycl::malloc_shared(global_range*sizeof(int), dev, ctex));

  std::cout << "Running on "
            << myQueue.get_device().get_info<sycl::info::device::name>()
            << "\n";

  //Create a command_group to issue command to the group
  myQueue.submit([&](sycl::handler& cgh) {
     //Create an accesor for the sycl buffer. Trust me, use auto.
    // Nd range allow use to access information
    cgh.parallel_for<class hello_world>(sycl::nd_range<1>{sycl::range<1>(global_range), 
                                                              sycl::range<1>(local_range) }, 
                                        [=](sycl::nd_item<1> idx) {
      const int world_rank = idx.get_global_id(0);
      A[world_rank] = world_rank;
    }); // End of the kernel function
  }); // End of the queue commands
 myQueue.wait();

 for (size_t i = 0; i < global_range; i++) 
        std::cout<< "A[ " << i <<" ] = " << A[i] << std::endl;
  return 0;
}
