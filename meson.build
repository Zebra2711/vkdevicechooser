project(
    'vkdevicechooser',
    'cpp',
    version: '1',
    license: 'GPL',
)

cpp = meson.get_compiler('cpp')

if not cpp.has_header('vulkan/vulkan.h')
    error('Vulkan development files not present (vulkan/vulkan.h)')
endif
if not cpp.has_header('vulkan/vk_layer.h')
    error('Vulkan development files not present (vulkan/vk_layer.h)')
endif
if not cpp.has_header('vulkan/utility/vk_dispatch_table.h')
    error('Vulkan development files not present (vulkan/utility/vk_dispatch_table.h)')
endif

libdir = get_option('libdir')
datadir = get_option('datadir')
vulkanlayerdir = join_paths(datadir, 'vulkan', 'implicit_layer.d')

lib_src = files(
    'layer.cpp',
    'vk_layer_table.cpp',
    'vk_layer_table.h'
)

json_src = files(
    'vkdevicechooser.json',
)

vkdevicechooser_lib = library(
    'vkdevicechooser',
    lib_src,
    install: true,
    install_dir: libdir,
)

install_data(
    json_src,
    install_dir: vulkanlayerdir,
)
