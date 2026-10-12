FGFLAGS=-std=c11 -g -O0 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer $(VULKAN_CFLAGS) -Isrc
.PHONY: check331
check331:
	mkdir -p build/331-tests
	$(CC) $(FGFLAGS) src/compute_check.c tests/test_compute_check.c -o build/331-tests/compute-check
	./build/331-tests/compute-check
	$(CC) $(FGFLAGS) src/compute_commands.c tests/test_compute_commands.c -o build/331-tests/compute-commands
	./build/331-tests/compute-commands
	$(CC) $(FGFLAGS) src/compute_commands.c src/dispatch_encode.c tests/test_dispatch_encode.c -o build/331-tests/dispatch
	./build/331-tests/dispatch
	$(CC) $(FGFLAGS) src/descriptor_encode.c src/texture_format.c src/texture_descriptor.c src/texture_layout.c src/depth_layout.c tests/test_descriptor_encode.c -o build/331-tests/descriptor
	./build/331-tests/descriptor
	$(CC) $(FGFLAGS) $(VK_MEMORY_SOURCES) tests/test_vk_memory.c -o build/331-tests/memory
	./build/331-tests/memory
	$(CC) $(FGFLAGS) $(VK_QUEUE_SOURCES) tests/test_vk_queue.c -o build/331-tests/queue
	./build/331-tests/queue
	$(CC) $(FGFLAGS) $(VK_QUEUE_SOURCES) tests/test_coherent_memory.c -o build/331-tests/coherence
	./build/331-tests/coherence
	$(CC) $(FGFLAGS) src/compilation_cache.c tests/test_compilation_cache.c -o build/331-tests/cache
	./build/331-tests/cache
	$(CC) -std=c11 -g -O0 -Wall -Wextra -Werror -fsanitize=undefined $(NATIVE_PREPARE_TEST) -o build/331-tests/native-prepare
	./build/331-tests/native-prepare
	$(CC) -std=c11 -g -O0 -Wall -Wextra -Werror -fsanitize=undefined $(VULKAN_CFLAGS) -Isrc -I$(LAB_SIBLINGS)/ps5-agc-gears/include -I$(LAB_SIBLINGS)/logging_server/client native/memory_ps5.c tests/test_native_memory_alignment.c -o build/331-tests/alignment
	./build/331-tests/alignment
