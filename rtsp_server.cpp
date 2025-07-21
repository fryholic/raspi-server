// RTSP 송수신 모듈 of 서버 Pi 
// 엣지 디바이스 Pi로부터 영상 수신, 클라이언트 Windows로 영상 송신
// 이 코드의 RTSP 서버 주소 : rtsp://<ip주소>:8554/retransmit

#include "rtsp_server.hpp"

#define DEFAULT_RTSP_PORT "8554"
#define DEFAULT_MOUNT_POINT "/retransmit"

void rtsp_run(int argc, char *argv[]){
    // GStreamer 초기화 (초기화 실패시 에러 발생시킴)
    GError *error = NULL;
    gboolean initialized = gst_init_check(&argc,&argv,&error);
    if(!initialized){
        g_printerr("GStreamer 초기화 실패: %s\n", error->message);
        g_error_free(error);
        return;
    }

    // RTSP 서버 생성
    GstRTSPServer *server = gst_rtsp_server_new();

    // RTSP 마운트 포인트 획득
    // 마운트 포인트 : rtsp://<ip주소>:<포트번호>/<이 부분>r
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);

    // RTSP 미디어 팩토리 생성
    // 미디어 팩토리 : 클라이언트의 요청을 받으면 실제 미디어 스트림을 만들어주는 요소
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();

    // RTSP 수신 & 송신 파이프라인 생성
    const char *pipeline_description =
    "rtspsrc location=rtsp://admin:admin123%40@192.168.0.137:554/0/onvif/profile2/media.smp ! " // 엣지 디바이스 Pi 주소 수정 필요, latency : 버퍼링 지연 시간 결정
    "rtph264depay ! " // RTP 패킷 -> H.264 비디오 데이터 추출(depacketize)
    "h264parse ! " // H.264 비디오 데이터 파싱
    "rtph264pay name=pay0 pt=96"; // 비디오 데이터 -> RTP 패킷화
    gst_rtsp_media_factory_set_launch(factory,pipeline_description);

    // RTSP 마운트 포인트 재설정 및 팩토리 추가
    gst_rtsp_mount_points_add_factory(mounts, "/retransmit", factory);
    g_object_unref(mounts);

    // RTSP 서버 시작
    if (gst_rtsp_server_attach(server, NULL) == 0) {
        cerr << "RTSP 서버 Attach 실패\n";
        return;
    }

    cout << "RTSP middle server is running \n";

    // 메인 루프 실행
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    return;

}

void pi_rtsp_run(int argc, char *argv[]){
    GMainLoop *loop;
    GstRTSPServer *server;
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;
    gchar *port = (gchar *) DEFAULT_RTSP_PORT;
    gchar *source_rtsp_url;

    // Check command line arguments
    if (argc != 2) {
        g_printerr("Usage: %s <source-rtsp-url>\n", argv[0]);
        g_printerr("Example: %s rtsp://<pi-ip-address>:8554/stream\n", argv[0]);
        return;
    }
    source_rtsp_url = "rtsp://~";

    // Initialize GStreamer
    gst_init(&argc, &argv);

    loop = g_main_loop_new(NULL, FALSE);

    // Create a new RTSP server
    server = gst_rtsp_server_new();
    g_object_set(server, "service", port, NULL);

    // Get the mount points for the server
    mounts = gst_rtsp_server_get_mount_points(server);

    // Create a media factory
    factory = gst_rtsp_media_factory_new();

    // Construct the pipeline string for re-streaming
    // This pipeline connects to the source RTSP, depayloads, parses, and rep payloads it for new clients
    gchar *launch_string = g_strdup_printf(
        "( rtspsrc location=%s latency=0 ! rtph264depay ! h264parse ! rtph264pay name=pay0 pt=96 )",
        source_rtsp_url
    );

    gst_rtsp_media_factory_set_launch(factory, launch_string);
    g_free(launch_string);

    // Set the factory to be shared. This means one source pipeline for all clients.
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    // Add the factory to the mount points
    gst_rtsp_mount_points_add_factory(mounts, DEFAULT_MOUNT_POINT, factory);
    g_object_unref(mounts);

    // Attach the server to the main context
    if (gst_rtsp_server_attach(server, NULL) == 0) {
        g_printerr("Failed to attach RTSP server.\n");
        return;
    }

    g_print("RTSP re-stream server is ready.\n");
    g_print("Source URL: %s\n", source_rtsp_url);
    g_print("Stream available at: rtsp://127.0.0.1:%s%s\n", port, DEFAULT_MOUNT_POINT);

    // Run the main loop
    g_main_loop_run(loop);

    // Clean up
    g_main_loop_unref(loop);
    g_object_unref(server);

    return;
}