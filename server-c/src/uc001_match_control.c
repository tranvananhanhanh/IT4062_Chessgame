// UC001: Xử lý Giao thức Điều khiển Trận đấu (Match Control)

#include <stdio.h>
#include <stdlib.h>
#include "include/protocol_common.h"

// Khai báo hàm mock để đảm bảo liên kết
void send_to_client(int client_fd, const char *message);
void update_database(const char *query);


/**
 * Xử lý UC001: Điều khiển Trận đấu (Match Control)
 * Áp dụng logic chuyển trạng thái và phản hồi theo thiết kế.
 */
void handle_match_control(GameSession *session, CommandType cmd, int client_fd, int opponent_fd) {
    // Giả định: client_fd là người gửi lệnh, opponent_fd là đối thủ

    printf("[UC001] Handling command %d in state %d\n", cmd, session->current_state);

    // Xử lý các lệnh chỉ hợp lệ ở trạng thái IN_GAME
    if (session->current_state == STATE_IN_GAME) {
        switch (cmd) {
            case CMD_PAUSE:
                session->current_state = STATE_PAUSED;
                send_to_client(client_fd, "OK GAME PAUSED");
                send_to_client(opponent_fd, "GAME PAUSED BY OPPONENT");
                return;
            case CMD_SURRENDER:
                session->current_state = STATE_GAME_OVER;
                send_to_client(client_fd, "RESULT LOSE");
                send_to_client(opponent_fd, "RESULT WIN SURRENDER");
                // Cập nhật DB: UPDATE match_game SET result='white_win', winner_id=opponent_id...
                update_database("UPDATE match_game SET status='finished', result='SURRENDER'");
                return;
            case CMD_DRAW:
                session->current_state = STATE_WAIT_DRAW_RESP;
                send_to_client(client_fd, "REQ DRAW SENT");
                send_to_client(opponent_fd, "REQ DRAW FROM OPPONENT");
                return;
            default:
                break; // Xử lý lỗi ở cuối hàm
        }
    }

    // Xử lý các lệnh chỉ hợp lệ ở trạng thái PAUSED
    if (session->current_state == STATE_PAUSED) {
        if (cmd == CMD_RESUME) {
            session->current_state = STATE_IN_GAME;
            send_to_client(client_fd, "OK GAME RESUMED");
            send_to_client(opponent_fd, "GAME RESUMED");
            return;
        }
    }

    // Xử lý các lệnh chỉ hợp lệ ở trạng thái WAIT_DRAW_RESP (Phản hồi từ đối thủ)
    if (session->current_state == STATE_WAIT_DRAW_RESP) {
        // Cần xác minh rằng lệnh chấp nhận/từ chối phải đến từ đối thủ (client_fd != người đề nghị)
        // Ở đây giả định client_fd là người gửi lệnh hiện tại
        if (cmd == CMD_DRAW_ACCEPT) {
            session->current_state = STATE_GAME_OVER;
            send_to_client(client_fd, "RESULT DRAW"); // Gửi cho người chấp nhận
            send_to_client(opponent_fd, "RESULT DRAW"); // Gửi cho người đề nghị (đã được đổi vai trò trong lệnh)
            update_database("UPDATE match_game SET status='finished', result='DRAW'");
            return;
        }
        if (cmd == CMD_DRAW_DECLINE) {
            session->current_state = STATE_IN_GAME;
            send_to_client(client_fd, "OK CONTINUE"); // Gửi cho người từ chối
            send_to_client(opponent_fd, "OK CONTINUE"); // Gửi cho người đề nghị
            return;
        }
    }

    // Xử lý các lệnh chỉ hợp lệ ở trạng thái GAME_OVER (yêu cầu đấu lại)
    if (session->current_state == STATE_GAME_OVER) {
        if (cmd == CMD_REMATCH) {
            session->current_state = STATE_WAIT_REMATCH_RESP;
            send_to_client(client_fd, "REQ REMATCH SENT");
            send_to_client(opponent_fd, "REQ REMATCH FROM OPPONENT");
            return;
        }
    }

    // Xử lý các lệnh chỉ hợp lệ ở trạng thái WAIT_REMATCH_RESP
    if (session->current_state == STATE_WAIT_REMATCH_RESP) {
        // Cần xác minh rằng lệnh chấp nhận/từ chối phải đến từ đối thủ
        if (cmd == CMD_REMATCH_ACCEPT) {
            // Khởi tạo ván mới
            session->current_state = STATE_IN_GAME;
            session->match_id = session->match_id + 1; // Giả lập ID ván mới
            strncpy(session->current_fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", sizeof(session->current_fen));
            send_to_client(client_fd, "NEW MATCH STARTED");
            send_to_client(opponent_fd, "NEW MATCH STARTED");
            update_database("INSERT INTO match_game(type='pvp', status='in_progress')");
            return;
        }
        if (cmd == CMD_REMATCH_DECLINE) {
            session->current_state = STATE_GAME_OVER; // Vẫn ở trạng thái kết thúc
            send_to_client(client_fd, "OK REMATCH DENIED");
            send_to_client(opponent_fd, "OK REMATCH DENIED");
            return;
        }
    }

    // Xử lý lỗi chung (INVALID ACTION)
    send_to_client(client_fd, "ERROR INVALID ACTION");
}
