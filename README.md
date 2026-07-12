# HFT Core: Zero-Allocation Deterministic Execution Gateway

## Architecture Overview
This repository contains a high-performance, kernel-bypass execution client for the Solana network.

## Core Features
* Borsh-to-C Zero-Copy Deserialization
* Kernel-Bypass Egress
* Deterministic Latency

## Integration
This client utilizes the core execution engine.

## Quick Start
gcc -O3 -Wall -std=c11 -Iinclude src/solana_adapter.c tests/test_solana_tpu.c -o solana_tpu_client
./solana_tpu_client

## Grant Milestone Roadmap
* Milestone 1: Borsh-to-C deserializer and state sync verification.
* Milestone 2: TPU injection and Ed25519 optimized signing loop.
* Milestone 3: Mainnet shadow-mode deployment and security audit.
