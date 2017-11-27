#!/usr/bin/env python
# The first few lines make the locally build module available in this python script!
from __future__ import print_function, division
import sys, os
my_dir = os.path.dirname(os.path.realpath(__file__))
print(my_dir)

RESULTS_DIR = os.path.join(my_dir, 'results')

import numpy as np
import time
import datetime
import matplotlib.pyplot as plt
import pysymboldemapper as sd
import channel_simulation as cs
import pypolar

from polar_5g_parameters import get_polar_5g_frozenBitPositions, design_snr_to_bec_eta, calculate_bec_channel_capacities, get_frozenBitPositions


def generate_filename(file_prefix, constellation_order, inv_coderate, scl_list_size, info_length, ebn0):
    ts = '{:%Y-%m-%d-%H:%M:%S}'.format(datetime.datetime.now())
    result_filename = '{:s}_ConstOrder{}_invRate{}_ListSize{}_InfoLength{}_EBN{:.2f}_{:s}'.format(file_prefix, constellation_order, inv_coderate, scl_list_size, info_length, ebn0, ts)
    return os.path.join(RESULTS_DIR, result_filename)


def save_results(filename, constellation_order, inv_coderate, info_length, ebn0, n_iterations, scl_list_size, python_ns,
                 c_ns, results):
    d = {
        'constellation_order': constellation_order,
        'invCoderate': inv_coderate,
        'info_length': info_length,
        'scl_list_size': scl_list_size,
        'EbN0': ebn0,
        'iterations': n_iterations,
        'avgDecoderDurationNs': {'python': python_ns, 'c': c_ns},
        'results': results
    }
    # np.save(filename, d)
    return d


def calculate_ber(results, info_length):
    n_errors = (len(results) * info_length) - np.sum(results)
    return n_errors / (len(results) * info_length)


def calculate_fer(results, info_length):
    n_correct_frames = np.sum(results == info_length)
    return 1. * (len(results) - n_correct_frames) / len(results)


def print_time_statistics(datapoint_sim_duration, python_ns, c_ns, enc_c_ns, n_iterations, codeword_len, info_length):
    datapoint_sim_duration_per_iteration = int(datapoint_sim_duration * 1e9 / n_iterations)
    python_ns /= n_iterations
    c_ns /= n_iterations
    enc_c_ns /= n_iterations
    p_s = 1e-9 * c_ns
    py_info_thr = 1.e-6 * info_length / p_s
    py_code_thr = 1.e-6 * codeword_len / p_s
    encoder_thr = 1.e-6 * info_length / (1.e-9 * enc_c_ns)
    print('Simulation Duration: {:6.2f}s,\tper iteration: {:6.2f}us'.format(datapoint_sim_duration, datapoint_sim_duration_per_iteration / 1e3))
    print('Decoder      Python: {:6.2f}us,\tC: {:6.2f}us,\tInterface: {:6.2f}us, {}'.format(python_ns / 1e3, c_ns / 1e3, (python_ns - c_ns) / 1e3, enc_c_ns))
    print('Decoder  Throughput: {:6.2f}Mbit/s Info, {:6.2f}Mbit/s Code\tEncoder {:6.2f}Mbit/s'.format(py_info_thr, py_code_thr, encoder_thr))


def lte_subblock_interleaver_indices(block_indices):
    # subblock interleaver permutation pattern. cf. TS36.212 Table 5.1.4-4
    permutation_pattern = np.array([0, 16, 8, 24, 4, 20, 12, 28, 2, 18, 10, 26, 6, 22, 14, 30, 1, 17, 9, 25, 5, 21, 13, 29, 3, 19, 11, 27, 7, 23, 15, 31], dtype=np.int32)
    # assert subblock_index_matrix.shape[0] == 3  # This assures that matrix dimensions are as expected!
    subblock_len = len(block_indices)
    row_len = int(np.ceil(subblock_len / 32))
    dummy_len = 32 * row_len - subblock_len

    # prepend NaNs
    subblock = np.hstack((np.full((dummy_len, ), np.NaN), block_indices))

    subblock_interleaver0 = np.arange(32 * row_len)
    subblock_interleaver0 = np.reshape(subblock_interleaver0, (-1, 32))
    subblock_interleaver0 = subblock_interleaver0[:, permutation_pattern]
    subblock_interleaver0 = subblock_interleaver0.T.flatten()
    indices = subblock[subblock_interleaver0]
    indices = indices[np.where(np.invert(np.isnan(indices)))]
    indices = indices.astype(int)
    assert np.all(np.sort(indices) == np.arange(len(indices), dtype=indices.dtype))
    return indices


def validate_interleaver_padding_combo(bit_interleaver, N):
    codeword_len = len(bit_interleaver)
    bits = np.arange(N)
    tx = np.concatenate((bits, np.zeros(codeword_len - N, dtype=bits.dtype)))
    tx = tx[bit_interleaver]

    llrs = np.zeros(codeword_len, dtype=tx.dtype)
    llrs[bit_interleaver] = tx
    llrs = llrs[0:N]
    assert np.all(bits == llrs)


def simulate_awgn_channel(tx, modulation_constellation, demapper, sigma):
    tx_symbols = sd.map_to_constellation(tx, modulation_constellation)
    noise = cs.get_complex_noise_vector(len(tx_symbols), sigma)
    rx = tx_symbols + noise
    rx_llrs = demapper.demap_symbols(rx, modulation_constellation, 1. / (sigma ** 2))
    return rx_llrs


def simulate_awgn_polar_code_err_frames(constellation_order, inv_coderate, info_length, ebn0, max_frame_errs=8,
                                        scl_list_size=8):
    filename = generate_filename('PolarCodeAWGN', constellation_order, inv_coderate, scl_list_size, info_length, ebn0)

    K = info_length
    N = info_length * inv_coderate

    # if N <= 1024:
    #     f = get_polar_5g_frozenBitPositions(N, N - K)
    # else:
    # eta = design_snr_to_bec_eta(0.0, 1.0)
    # polar_capacities = calculate_bec_channel_capacities(eta, N)
    # f = get_frozenBitPositions(polar_capacities, N - K)
    f = pypolar.frozen_bits(N, K, 0.0)
    encoder = pypolar.PolarEncoder(N, f)
    decoder = pypolar.PolarDecoder(N, scl_list_size, f, 'float')
    # decoder.enableSoftOutput(True)

    sigma = cs.ebn0_to_sigma(ebn0, 1. / inv_coderate, constellation_order)
    print('EbN0 {:.2f}dB -> SNR {:.2f}dB'.format(ebn0, 10. * np.log10(1. / sigma ** 2)))

    codeword_len = int(np.ceil(1. * N / constellation_order) * constellation_order)
    bit_interleaver = lte_subblock_interleaver_indices(np.arange(codeword_len))
    validate_interleaver_padding_combo(bit_interleaver, N)

    mod_constell, bit_representation = sd.generate_gray_constellation(constellation_order)
    demapper = sd.pysymboldemapper()

    python_ns = 0
    c_ns = 0
    enc_c_ns = 0
    err_frames = 0
    n_iterations = 0
    max_iterations = max_frame_errs * (2 ** 10)
    results = np.zeros(max_iterations)
    error_pattern = np.zeros(info_length)
    sim_start_time = time.time()
    while err_frames < max_frame_errs and n_iterations < max_iterations:
        bits = np.random.randint(0, 2, info_length).astype(np.int32)
        d = np.packbits(bits)
        encoded = encoder.encode_vector(d)
        enc_c_ns += encoder.encoder_duration()

        btx = np.unpackbits(encoded)
        tx = np.concatenate((btx, np.zeros(codeword_len - N, dtype=btx.dtype)))
        tx = tx[bit_interleaver]

        rx_llrs = simulate_awgn_channel(tx, mod_constell, demapper, sigma).astype(dtype=np.float32)

        llrs = np.zeros(codeword_len, dtype=rx_llrs.dtype)
        llrs[bit_interleaver] = rx_llrs
        llrs = llrs[0:N]
        # llrs = llrs.astype(dtype=np.float32)

        st = time.time()
        up = decoder.decode_vector(llrs)
        python_ns += int((time.time() - st) * 1e9)
        c_ns += decoder.decoder_duration()
        u = np.unpackbits(up)

        frame_pattern = bits == u
        error_pattern += (1 - frame_pattern)
        n_correct = np.sum(frame_pattern)
        results[n_iterations] = n_correct

        n_iterations += 1
        if not n_correct == info_length:
            err_frames += 1
        fer = 1. * err_frames / n_iterations
        if n_iterations % (2 ** 13) == 0:
            sim_perc = 100. * n_iterations / max_iterations
            # fer = calculate_fer(results[0:n_iterations], info_length)
            print('simulating {}/{} frames time {:.2f}s {:.2f}% {}\tFER={:8.2e}'.format(err_frames, max_frame_errs,  time.time() - sim_start_time, sim_perc, n_iterations, fer))
        if n_iterations > 8 * max_frame_errs and fer < 1e-4:
            print('stop simulation for data point')
            break
    datapoint_sim_duration = time.time() - sim_start_time
    results = results[0:n_iterations]
    print_time_statistics(datapoint_sim_duration, python_ns, c_ns, enc_c_ns, n_iterations, codeword_len, info_length)

    ber = calculate_ber(results, info_length)
    fer = calculate_fer(results, info_length)

    print('has Softoutput: ', decoder.hasSoftOutput())
    # softoutout = decoder.getSoftInformation()
    # print(np.shape(softoutout))
    # print(softoutout)
    # print(error_pattern)
    # plt.plot(error_pattern)

    print('#frames {} with BER {:8.2e} and FER {:8.2e}'.format(len(results), ber, fer))
    return save_results(filename, constellation_order, inv_coderate, info_length, ebn0, n_iterations, scl_list_size,
                        python_ns, c_ns, results)


def polar_scl_simulation_run():
    info_len_list = np.array([32, 64, 128, 256, 512, 1024, ], dtype=int)
    info_len_list = np.array([32, 64, 128, ], dtype=int)
    info_len_list = np.array([64, 128, ], dtype=int)
    info_len_list = np.array([512, 2048, ], dtype=int)
    info_len_list = np.array([int(2 ** 8), ], dtype=int)
    print(info_len_list)
    inv_coderate = 2
    ebn0_list = np.arange(0.0, 3.25, .25)  # overall
    # ebn0_list = np.arange(0.7, 2.0, .1)  # overall
    # ebn0_list = np.arange(-7.0, -3.25, .25)  # BPSK
    # ebn0_list = np.arange(-2.0, 4.25, .25)  # QPSK
    # ebn0_list = np.arange(4.0, 8.25, .25)  # 8-PSK
    # ebn0_list = np.arange(4.0, 4.55, .25)  # overall
    fer_list = np.ones(len(ebn0_list))
    ber_list = np.ones(len(ebn0_list))
    num_err_frames = 2 ** 9
    scl_size_list = 2 ** np.arange(4)
    print(scl_size_list)
    constellation_order = 2


    plt.ion()
    plt.semilogy(ebn0_list, fer_list)
    my_figure = plt.gcf()
    plt.draw()
    plt.grid()

    for scl_size in scl_size_list:
        for info_length in info_len_list:
            for i, ebn0 in enumerate(ebn0_list):
                print('\nConstellationOrder {}, invCoderate {}, SCL List size {}, info length {}, EbN0 {:.2f}'.format(constellation_order, inv_coderate, scl_size, info_length, ebn0))
                res = simulate_awgn_polar_code_err_frames(constellation_order, inv_coderate, info_length, ebn0,
                                                          num_err_frames, scl_size)
                results = res['results']
                fer_list[i] = calculate_fer(results, info_length)
                ber_list[i] = calculate_ber(results, info_length)

                my_figure.clear()
                plt.semilogy(ebn0_list, fer_list)
                plt.semilogy(ebn0_list, ber_list)
                plt.xlabel('Eb/N0 [dB]')
                plt.title('Polar Code ({}, {}), constellation order {}'.format(int(info_length * inv_coderate), info_length, constellation_order))
                plt.grid()
                plt.draw()
    plt.ioff()
    plt.show()


def polar_simulation_run():
    info_len_list = np.array([32, 64, 128, 256, 512, 1024, ], dtype=int)
    info_len_list = np.array([32, 64, 128, ], dtype=int)
    info_len_list = np.array([64, 128, ], dtype=int)
    info_len_list = np.array([512, 2048, ], dtype=int)
    info_len_list = np.array([int(2 ** 16), ], dtype=int)
    print(info_len_list)
    inv_coderate = 2
    ebn0_list = np.arange(-4.0, 5.25, .25)  # overall
    ebn0_list = np.arange(0.7, 2.0, .1)  # overall
    # ebn0_list = np.arange(-7.0, -3.25, .25)  # BPSK
    # ebn0_list = np.arange(-2.0, 4.25, .25)  # QPSK
    # ebn0_list = np.arange(4.0, 8.25, .25)  # 8-PSK
    # ebn0_list = np.arange(4.0, 4.55, .25)  # overall
    fer_list = np.ones(len(ebn0_list))
    ber_list = np.ones(len(ebn0_list))
    num_err_frames = 2 ** 9
    scl_list_size = 1

    plt.ion()
    plt.semilogy(ebn0_list, fer_list)
    my_figure = plt.gcf()
    plt.draw()
    plt.grid()

    for constellation_order in range(2, 3):
        for info_length in info_len_list:
            for i, ebn0 in enumerate(ebn0_list):
                print('\nConstellationOrder {}, invCoderate {}, SCL List size {}, info length {}, EbN0 {:.2f}'.format(constellation_order, inv_coderate, scl_list_size, info_length, ebn0))
                res = simulate_awgn_polar_code_err_frames(constellation_order, inv_coderate, info_length, ebn0,
                                                          num_err_frames, scl_list_size)
                results = res['results']
                fer_list[i] = calculate_fer(results, info_length)
                ber_list[i] = calculate_ber(results, info_length)

                my_figure.clear()
                plt.semilogy(ebn0_list, fer_list)
                plt.semilogy(ebn0_list, ber_list)
                plt.xlabel('Eb/N0 [dB]')
                plt.title('Polar Code ({}, {}), constellation order {}'.format(int(info_length * inv_coderate), info_length, constellation_order))
                plt.grid()
                plt.draw()
    plt.ioff()
    plt.show()


def simulate_awgn_llr_distribution(constellation_order, inv_coderate, info_length, ebn0, max_frame_errs=8):
    filename = generate_filename('PolarCodeLLR', constellation_order, inv_coderate, 0, info_length, ebn0)

    K = info_length
    N = info_length * inv_coderate
    f = pypolar.frozen_bits(N, K, 0.0)

    encoder = pypolar.PolarEncoder(N, f)
    encoder.setErrorDetection()
    decoder1 = pypolar.PolarDecoder(N, 1, f, 'float')
    decoder2 = pypolar.PolarDecoder(N, 2, f, 'float')
    decoder4 = pypolar.PolarDecoder(N, 4, f, 'float')
    decoder8 = pypolar.PolarDecoder(N, 8, f, 'float')
    decoder1.enableSoftOutput(True)
    decoder2.enableSoftOutput(True)
    decoder4.enableSoftOutput(True)
    decoder8.enableSoftOutput(True)
    decoder1.setErrorDetection()
    decoder2.setErrorDetection()
    decoder4.setErrorDetection()
    decoder8.setErrorDetection()
    sigma = cs.ebn0_to_sigma(ebn0, 1. / inv_coderate, constellation_order)
    print('EbN0 {:.2f}dB -> SNR {:.2f}dB'.format(ebn0, 10. * np.log10(1. / sigma ** 2)))

    codeword_len = int(np.ceil(1. * N / constellation_order) * constellation_order)
    # bit_interleaver = lte_subblock_interleaver_indices(np.arange(codeword_len))
    # validate_interleaver_padding_combo(bit_interleaver, N)

    mod_constell, bit_representation = sd.generate_gray_constellation(constellation_order)
    demapper = sd.pysymboldemapper()

    python_ns = 0
    c_ns = 0
    enc_c_ns = 0
    err_frames = 0
    n_iterations = 0
    max_iterations = max_frame_errs * (2 ** 10)
    results = np.zeros(max_iterations)
    error_pattern = np.zeros(info_length)

    list_errs = np.zeros(3)
    llrs_list1 = np.array([])
    llrs_list2 = np.array([])
    llrs_list4 = np.array([])
    llrs_list8 = np.array([])
    llrs_listf = np.array([])

    min_corr_perf = np.zeros(max_iterations)

    sim_start_time = time.time()
    while err_frames < max_frame_errs and n_iterations < max_iterations:
        bits = np.random.randint(0, 2, info_length).astype(np.int32)
        d = np.packbits(bits)
        encoded = encoder.encode_vector(d)
        bits = np.unpackbits(d)

        enc_c_ns += encoder.encoder_duration()

        btx = np.unpackbits(encoded)
        # tx = np.concatenate((btx, np.zeros(codeword_len - N, dtype=btx.dtype)))
        tx = btx
        llrs = simulate_awgn_channel(tx, mod_constell, demapper, sigma).astype(dtype=np.float32)
        # print(llrs[0:15])
        # print(np.linalg.norm(llrs))
        llrs *= codeword_len / np.linalg.norm(llrs)
        # print(llrs[0:15])
        # print(np.linalg.norm(llrs))

        st = time.time()
        up = decoder1.decode_vector(llrs)
        python_ns += int((time.time() - st) * 1e9)
        c_ns += decoder1.decoder_duration()

        u = np.unpackbits(up)
        ellrs = decoder1.getSoftCodeword()
        sllrs = decoder1.getSoftInformation()

        rxcw = (llrs < 0.0).astype(btx.dtype)
        erxcw = (ellrs < 0.0).astype(btx.dtype)

        # print(btx[0:15])
        # print(rxcw[0:15])
        # print(erxcw[0:15])
        # print(llrs[0:15])
        # print(ellrs[0:15])
        rxctr = np.sum(rxcw == btx)
        erxctr = np.sum(erxcw == btx)
        min_corr_perf[n_iterations] = erxctr - rxctr
        # print(rxctr, erxctr, rxctr < erxctr, erxctr - rxctr)

        # bep = np.sum(1. / (1 + np.exp(np.abs(ellrs)))) / len(ellrs)
        # eellrs = np.linalg.norm(ellrs - llrs)
        # eellrs = bep
        eellrs = erxctr - rxctr

        frame_pattern = bits == u
        error_pattern += (1 - frame_pattern)
        n_correct = np.sum(frame_pattern)
        results[n_iterations] = n_correct
        # eellrs = n_correct

        n_iterations += 1
        if not n_correct == info_length:
            # print(np.sum(np.abs(llrs)), np.sum(np.abs(ellrs)), np.sum(np.abs(sllrs)))
            err_frames += 1
            # eellrs = np.sum(np.abs(ellrs))
            # eellrs = np.linalg.norm(sllrs)
            # bep = np.sum(1. / (1 + np.exp(np.abs(sllrs)))) / len(sllrs)

            up2 = decoder2.decode_vector(llrs)
            up4 = decoder4.decode_vector(llrs)
            up8 = decoder8.decode_vector(llrs)
            c2 = np.all(np.unpackbits(up2) == bits)
            c4 = np.all(np.unpackbits(up4) == bits)
            c8 = np.all(np.unpackbits(up8) == bits)
            list_errs[0] += c2
            list_errs[1] += c4
            list_errs[2] += c8
            # if c2 is False:
            #     llrs_list1 = np.append(llrs_list1, np.sum(np.abs(sllrs)))
            # llr_sum = np.sum(np.abs(eellrs))
            if c2:
                llrs_list2 = np.append(llrs_list2, eellrs)
            elif c4:
                llrs_list4 = np.append(llrs_list4, eellrs)
            elif c8:
                llrs_list8 = np.append(llrs_list8, eellrs)
            else:
                llrs_listf = np.append(llrs_listf, eellrs)
               # print('pattern: 2:{}, 4:{}, 8:{}'.format(c2, c4, c8))
        else:
            llrs_list1 = np.append(llrs_list1, eellrs)

        fer = 1. * err_frames / n_iterations
        if n_iterations % (2 ** 13) == 0:
            sim_perc = 100. * n_iterations / max_iterations
            # fer = calculate_fer(results[0:n_iterations], info_length)
            print('simulating {}/{} frames time {:.2f}s {:.2f}% {}\tFER={:8.2e}'.format(err_frames, max_frame_errs,  time.time() - sim_start_time, sim_perc, n_iterations, fer))
        if n_iterations > 8 * max_frame_errs and fer < 1e-4:
            print('stop simulation for data point')
            break
    datapoint_sim_duration = time.time() - sim_start_time
    results = results[0:n_iterations]
    min_corr_perf = min_corr_perf[0:n_iterations]
    print_time_statistics(datapoint_sim_duration, python_ns, c_ns, enc_c_ns, n_iterations, codeword_len, info_length)
    ber = calculate_ber(results, info_length)
    fer = calculate_fer(results, info_length)
    print('#frames {} with BER {:8.2e} and FER {:8.2e}'.format(len(results), ber, fer))
    print(list_errs)
    print(np.mean(llrs_list1), np.mean(llrs_list2), np.mean(llrs_list4), np.mean(llrs_list8), np.mean(llrs_listf))
    print(np.var(llrs_list1), np.var(llrs_list2), np.var(llrs_list4), np.var(llrs_list8), np.var(llrs_listf))
    print(np.std(llrs_list1), np.std(llrs_list2), np.std(llrs_list4), np.std(llrs_list8), np.std(llrs_listf))
    print(min_corr_perf)
    print(np.sum(min_corr_perf < 0), np.mean(min_corr_perf))
    plt.plot(min_corr_perf)
    # plt.errorbar(x=np.arange(5), y=[np.mean(llrs_list1), np.mean(llrs_list2), np.mean(llrs_list4), np.mean(llrs_list8), np.mean(llrs_listf)], yerr=[np.std(llrs_list1), np.std(llrs_list2), np.std(llrs_list4), np.std(llrs_list8), np.std(llrs_listf)])
    # plt.xlim((-.5, 4.5))
    plt.show()

    # return save_results(filename, constellation_order, inv_coderate, info_length, ebn0, n_iterations, 0,
    #                     python_ns, c_ns, results)


def simulate_awgn_turbo_polar_code(constellation_order, code_length, info_length, ebn0, max_frame_errs=8):
    inv_coderate = 1. * code_length / info_length
    filename = generate_filename('PolarCodeLLR', constellation_order, inv_coderate, 0, info_length, ebn0)

    K = info_length
    N = info_length * inv_coderate
    assert N == code_length
    frozen_bit_set = pypolar.frozen_bits(N, K, 0.0)
    info_bit_set = np.setdiff1d(np.arange(N, dtype=frozen_bit_set.dtype), frozen_bit_set)
    print(info_bit_set)
    print(code_length, info_length, N, K, inv_coderate)
    tpc_code_length = int(N + (N - K))
    tpc_coderate = K / tpc_code_length
    print(tpc_code_length, tpc_coderate)

    encoder = pypolar.PolarEncoder(N, frozen_bit_set)
    decoder0 = pypolar.PolarDecoder(N, 1, frozen_bit_set, 'float')
    decoder0.enableSoftOutput(True)
    decoder1 = pypolar.PolarDecoder(N, 1, frozen_bit_set, 'float')
    decoder1.enableSoftOutput(True)

    sigma = cs.ebn0_to_sigma(ebn0, tpc_coderate, constellation_order)
    print('EbN0 {:.2f}dB -> SNR {:.2f}dB'.format(ebn0, 10. * np.log10(1. / sigma ** 2)))

    codeword_len = int(np.ceil(1. * N / constellation_order) * constellation_order)
    bit_interleaver = lte_subblock_interleaver_indices(np.arange(info_length))
    # validate_interleaver_padding_combo(bit_interleaver, N)

    mod_constell, bit_representation = sd.generate_gray_constellation(constellation_order)
    demapper = sd.pysymboldemapper()


    python_ns = 0
    c_ns = 0
    enc_c_ns = 0
    err_frames = 0
    n_iterations = 0
    max_iterations = max_frame_errs * (2 ** 10)
    results = np.zeros(max_iterations)

    ref_err_ctr = 0

    sim_start_time = time.time()
    while err_frames < max_frame_errs and n_iterations < max_iterations:
        bits = np.random.randint(0, 2, info_length).astype(np.int32)
        ibits = bits[bit_interleaver]
        d0 = np.packbits(bits)
        d1 = np.packbits(ibits)
        encoded0 = encoder.encode_vector(d0)
        encoded1 = encoder.encode_vector(d1)
        enc_c_ns += encoder.encoder_duration()

        btx0 = np.unpackbits(encoded0)
        btx1 = np.unpackbits(encoded1)
        assert np.all(btx0[info_bit_set] == bits)
        assert np.all(btx0[info_bit_set][bit_interleaver] == btx1[info_bit_set])

        ptx1 = btx1[frozen_bit_set]

        btx = np.concatenate((btx0, ptx1))
        # tx = np.concatenate((btx, np.zeros(codeword_len - N, dtype=btx.dtype)))
        tx = btx
        llrs = simulate_awgn_channel(tx, mod_constell, demapper, sigma).astype(dtype=np.float32)
        llrs0 = np.copy(llrs[0:N])

        llrs1 = np.zeros(N, dtype=llrs.dtype)
        llrs1[frozen_bit_set] = llrs[N:]

        tmp_ellrs = np.zeros(K, dtype=llrs1.dtype)

        st = time.time()
        it_num = 0
        for i in range(8):
            up0 = decoder0.decode_vector(llrs0)

            ellrs0 = decoder0.getSoftInformation()
            escale = np.sum(np.abs(llrs0[info_bit_set])) / np.sum(np.abs(ellrs0))
            ellrs0 *= escale

            llrs1[info_bit_set] = ellrs0[bit_interleaver]
            up1 = decoder1.decode_vector(llrs1)
            u1 = np.unpackbits(up1)
            if np.all(ibits == u1):
                it_num = i
                break

            ellrs1 = decoder1.getSoftInformation()
            escale = np.sum(np.abs(llrs1[info_bit_set])) / np.sum(np.abs(ellrs1))
            ellrs1 *= escale

            tmp_ellrs[bit_interleaver] = ellrs1
            llrs0[info_bit_set] = tmp_ellrs
        if it_num > 0:
            print(it_num)

        python_ns += int((time.time() - st) * 1e9)
        c_ns += decoder1.decoder_duration()

        u0 = np.unpackbits(up0)
        u1 = np.unpackbits(up1)
        tmp = np.zeros(len(u1), dtype=u1.dtype)
        tmp[bit_interleaver] = u1
        u1 = tmp

        if not np.all(bits == u0):
            ref_err_ctr += 1

        frame_pattern = bits == u1
        n_correct = np.sum(frame_pattern)
        results[n_iterations] = n_correct

        n_iterations += 1
        if not n_correct == info_length:
            err_frames += 1

        fer = 1. * err_frames / n_iterations
        if n_iterations % (2 ** 13) == 0:
            sim_perc = 100. * n_iterations / max_iterations
            # fer = calculate_fer(results[0:n_iterations], info_length)
            print('simulating {}/{} frames time {:.2f}s {:.2f}% {}\tFER={:8.2e}'.format(err_frames, max_frame_errs,  time.time() - sim_start_time, sim_perc, n_iterations, fer))
        if n_iterations > 8 * max_frame_errs and fer < 1e-4:
            print('stop simulation for data point')
            break
    datapoint_sim_duration = time.time() - sim_start_time
    results = results[0:n_iterations]
    print_time_statistics(datapoint_sim_duration, python_ns, c_ns, enc_c_ns, n_iterations, codeword_len, info_length)
    ber = calculate_ber(results, info_length)
    fer = calculate_fer(results, info_length)
    print('#frames {} with BER {:8.2e} and FER {:8.2e}'.format(len(results), ber, fer))
    print(ref_err_ctr)
    # print(list_errs)
    # print(np.mean(llrs_list2), np.mean(llrs_list4), np.mean(llrs_list8), np.mean(llrs_listf))
    # print(np.var(llrs_list2), np.var(llrs_list4), np.var(llrs_list8), np.var(llrs_listf))
    # print(np.std(llrs_list2), np.std(llrs_list4), np.std(llrs_list8), np.std(llrs_listf))
    #
    # plt.errorbar(x=np.arange(4), y=[np.mean(llrs_list2), np.mean(llrs_list4), np.mean(llrs_list8), np.mean(llrs_listf)], yerr=[np.std(llrs_list2), np.std(llrs_list4), np.std(llrs_list8), np.std(llrs_listf)])
    # plt.show()

    # return save_results(filename, constellation_order, inv_coderate, info_length, ebn0, n_iterations, 0,
    #                     python_ns, c_ns, results)


def main():
    np.set_printoptions(precision=2, linewidth=150)
    # polar_simulation_run()
    # polar_scl_simulation_run()
    simulate_awgn_llr_distribution(2, 2, 256, 2.0, int(2 ** 12))
    # simulate_awgn_turbo_polar_code(2, 512, 344, 4.0, int(2 ** 9))

if __name__ == '__main__':
    main()
