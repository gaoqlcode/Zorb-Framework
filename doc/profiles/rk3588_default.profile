# RK3588 payload default profile
# key=value

enable_vis=1
enable_nir=1
enable_tir=1
camera_count=3

tcp_control_port=19000
udp_vis_port=20001
udp_nir_port=20002
udp_tir_port=20003

width=1920
height=1080
fps=30
bitrate_kbps=4096
gop=30
sync_window_us=30000

frame_pool_block_count=64
frame_pool_block_size=1048576

capture_cpu=4
encode_cpu=5
network_cpu=6
control_cpu=2
