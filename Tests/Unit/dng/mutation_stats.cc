/*
 * Copyright (c) 2016 Steven H. Wu
 * Authors:  Steven H. Wu <stevenwu@asu.edu>
 *
 * This file is part of DeNovoGear.
 *
 * DeNovoGear is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define BOOST_TEST_MODULE dng::mutation_stats

#include <cstdlib>
#include <iostream>
#include <random>

#include "../boost_test_helper.h"
#include "fixture_trio_workspace.h"
#include <dng/mutation_stats.h>



const int NUM_TEST = 100;

struct MutationStatsFixture {
    std::mt19937 random_gen_mt{1}; //seed = 1

    std::string fixture;
    std::uniform_real_distribution<double> rand_unif_log;
    std::uniform_real_distribution<double> rand_unif;

    double min_prob = 0.01;
    MutationStatsFixture(std::string s = "TestMutationStats") : fixture(s) {

        rand_unif_log = std::uniform_real_distribution<double>(std::log(1e-20), 0);
        rand_unif = std::uniform_real_distribution<double>(0, 1);

        BOOST_TEST_MESSAGE("set up fixture: " << fixture);
    }

    ~MutationStatsFixture() {
        BOOST_TEST_MESSAGE("tear down fixture: " << fixture);
    }

};


BOOST_FIXTURE_TEST_CASE(test_set_mup, MutationStatsFixture) {

    MutationStats stats (min_prob);

    for (int t = 0; t <NUM_TEST; ++t) {

        double ln_mut = rand_unif_log(random_gen_mt);
        double ln_no_mut = rand_unif_log(random_gen_mt);

        double prob_mu = std::exp(ln_mut);
        double prob_no_mut = std::exp(ln_no_mut);

        double expected = 1 - (prob_no_mut / prob_mu);
        double expected_log =  - std::expm1(ln_no_mut - ln_mut);

        peel::workspace_t work_nomut;
        peel::workspace_t work_full;
        work_nomut.forward_result = ln_no_mut;
        work_full.forward_result = ln_mut;
        stats.CalculateMutationProb(work_nomut, work_full);
        BOOST_CHECK_CLOSE(expected, expected_log, 0.01);
        BOOST_CHECK_CLOSE(expected_log, stats.mup_, BOOST_CLOSE_PERCENTAGE_THRESHOLD);
    }

    for (int t = 0; t <NUM_TEST; ++t) {

        double prob_mu = rand_unif(random_gen_mt);
        double prob_no_mut = rand_unif(random_gen_mt);

        double ln_mu = std::log(prob_mu);
        double ln_no_mut = std::log(prob_no_mut);

        double expected = 1 - (prob_no_mut / prob_mu);

        peel::workspace_t work_nomut;
        peel::workspace_t work_full;
        work_nomut.forward_result = ln_no_mut;
        work_full.forward_result = ln_mu;
        stats.CalculateMutationProb(work_nomut, work_full);
        BOOST_CHECK_CLOSE(expected, stats.mup_, BOOST_CLOSE_PERCENTAGE_THRESHOLD);
    }

}


BOOST_AUTO_TEST_CASE(test_set_node) {

    double min_prob = 0.01;
    MutationStats stats (min_prob);
    int number_of_event = 20;
    int first_non_founder = 5;

    std::vector<double> event(number_of_event, hts::bcf::float_missing);

    for (int t = first_non_founder; t < event.size(); ++t) {
        event[t] = t;
    }

    double total = 0;
    for (int i = first_non_founder; i < event.size(); ++i) {
        total += event[i];
    }

    std::vector<float> expected_mu1p(number_of_event, hts::bcf::float_missing);
    for (int i = first_non_founder; i < event.size(); ++i) {
        expected_mu1p[i] = static_cast<float>(event[i] / total);
    }

    stats.SetNodeMup(event, first_non_founder);
    stats.SetNodeMu1p(event, total, first_non_founder);

    for (int i = 0; i < first_non_founder; ++i) {
        BOOST_CHECK(bcf_float_is_missing(stats.node_mup_[i]));
        BOOST_CHECK(bcf_float_is_missing(stats.node_mu1p_[i]));
    }

    for (int i = first_non_founder; i < event.size(); ++i) {
        BOOST_CHECK_EQUAL(i, stats.node_mup_[i]);
        BOOST_CHECK_EQUAL(expected_mu1p[i], stats.node_mu1p_[i]);
    }

}


BOOST_AUTO_TEST_CASE(test_record) {
    BOOST_TEST_MESSAGE("Not yet implemented!");
    //TODO: implement check on record_info/stats, hts::bcf::Variant
    // RecordSingleMutationStats(hts::bcf::Variant &record){
}



BOOST_FIXTURE_TEST_CASE(test_set_posterior_probabilities, TrioWorkspace) {

    dng::IndividualVector expected_probs;

    for (int i = 0; i < workspace.num_nodes; ++i) {
        if (workspace.upper[i].size() == 0) {
            //PR_NOTE(SW): HACK! some upper doesn't exist yet, error in DEBUG mode, but ok is RELEASE MODE
            workspace.upper[i] = DNG_INDIVIDUAL_BUFFER_ONES;
        }
        expected_probs.push_back(DNG_INDIVIDUAL_BUFFER_ZEROS);
        double sum = 0;
        for (int j = 0; j < 10; ++j) {
            expected_probs[i][j] = workspace.upper[i][j] * workspace.lower[i][j];
            sum += expected_probs[i][j];
        }
        expected_probs[i] /= sum;
    }
    double min_prob = 0.01;
    MutationStats stats(min_prob);
    stats.SetPosteriorProbabilities(workspace);

    for (int i = 0; i < workspace.num_nodes; ++i) {
        boost_check_close_vector(expected_probs[i],
                              stats.posterior_probabilities_[i]);
    }

}


//BOOST_AUTO_TEST_CASE(test_genotype_stats) {
//
//
//    FindMutations find_mutation {min_prob, pedigree, test_param_1};
//    MutationStats mutation_stats {min_prob};
//    FindMutations::stats_t mutation_stats;
//    find_mutation(read_depths, ref_index, mutation_stats);
//
//    const int acgt_to_refalt_allele[] = {-1, 2, 0, 1, -1};
//    const int refalt_to_acgt_allele[5] = {2, 3, 1, -1, -1};
//    const uint32_t n_alleles = 3;
//    const std::size_t ref_index = 2;
//    const std::size_t num_nodes = 5;
//    const std::size_t library_start = 2;
//
//    std::vector<float> expected_gp_scores{
//            0.999833, 0.000167058, 1.78739e-12, 2.75325e-11, 3.57372e-16, 4.51356e-20,
//            0.984574, 0.0154262, 1.66835e-10, 9.77602e-12, 1.67414e-14, 1.58815e-20,
//            0.868225, 0.131775, 7.45988e-16, 1.99601e-09, 2.97932e-17, 3.704e-26,
//            0.999837, 0.000163186, 2.11375e-15, 2.7779e-11, 4.22658e-19, 5.33792e-23,
//            0.984578, 0.0154223, 6.97054e-14, 9.45085e-12, 6.98069e-18, 2.94856e-24
//    };
//    std::vector<float> expected_gl_scores{
//            NAN, NAN, NAN, NAN, NAN, NAN,
//            NAN, NAN, NAN, NAN, NAN, NAN,
//            -6.74046, 0, -6.07991, -7.58973, -7.5555, -15.93,
//            0, -6.64246, -17.5301, -6.64246, -17.5301, -17.5301,
//            0, -4.667, -16.0119, -7.10524, -16.3122, -18.7879
//    };
//    std::vector<int> expected_best_genotype{2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
//    std::vector<int> expected_genotype_qualities{38, 18, 9, 38, 18};
//
//
//    stats.SetGenotypeRelatedStats(acgt_to_refalt_allele, refalt_to_acgt_allele,
//                                  n_alleles, num_nodes, library_start);
//
//        boost_check_close_vector(expected_gp_scores, stats.gp_scores_);
//        boost_check_equal_vector(expected_best_genotype, stats.best_genotypes_);
//        boost_check_equal_vector(expected_genotype_qualities, stats.genotype_qualities_);
//
//    BOOST_CHECK_EQUAL(expected_gl_scores.size(), stats.gl_scores_.size());
//    for (int i = 0; i < expected_gl_scores.size(); ++i) {
//        if (isnan(expected_gl_scores[i])) {
//            BOOST_CHECK(bcf_float_is_missing(stats.gl_scores_[i]));
//        }
//        else {
//            BOOST_CHECK_CLOSE(expected_gl_scores[i], stats.gl_scores_[i],
//                              BOOST_CLOSE_PERCENTAGE_THRESHOLD);
//        }
//    }
//
//}



