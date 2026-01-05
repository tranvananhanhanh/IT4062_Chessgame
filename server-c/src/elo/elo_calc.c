#include "elo_calc.h"
#include <math.h>

double elo_expected_score(int player_elo, int opponent_elo) {
    double exponent = (double)(opponent_elo - player_elo) / 400.0;
    return 1.0 / (1.0 + pow(10.0, exponent));
}

int elo_get_k_factor(int elo, int games_played) {
    if (games_played < 30) {
        return 40;
    } else if (elo > 2400) {
        return 10;
    } else {
        return 20;
    }
}

int elo_calculate_new_rating(int current_elo, double expected, double actual, int k_factor) {
    double change = k_factor * (actual - expected);
    int new_elo = current_elo + (int)round(change);
    if (new_elo < 100) new_elo = 100;
    return new_elo;
}
