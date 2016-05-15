
embree.CMakeFlags = \
	-DENABLE_STATIC_LIB=ON \
	-DENABLE_TUTORIALS=OFF \
	-DRTCORE_RAY_MASK=ON

embree.target=build/embree/libembree.a

build/embree/libembree.a:
	mkdir -p build
	mkdir -p build/embree
	cd build/embree && cmake ../../submodules/embree $(embree.CMakeFlags)
	cd build/embree && make embree $(NUM_THREADS)
