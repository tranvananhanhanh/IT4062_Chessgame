#ifndef ELO_CALC_H
#define ELO_CALC_H

double elo_expected_score(int player_elo, int opponent_elo);
int elo_get_k_factor(int elo, int games_played);
int elo_calculate_new_rating(int current_elo, double expected, double actual, int k_factor);

#endif
