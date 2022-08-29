//
// Created by Ankush J on 4/8/22.
//

#pragma once

#include "common.h"
#include "logger.h"
#include "bvar.h"

#include <memory>
#include <mpi.h>
#include <string>
#include <vector>

#define MPI_CHECK(status, msg) \
  if (status != MPI_SUCCESS) { \
    logf(LOG_ERRO, msg);       \
  }

struct NeighborBlock {
  int block_id;
  int peer_rank;
  int buf_id;
};

class MeshBlock : public std::enable_shared_from_this<MeshBlock> {
 public:
  MeshBlock(int block_id) : block_id_(block_id) {}
  MeshBlock(const MeshBlock& other)
      : block_id_(other.block_id_), nbrvec_(other.nbrvec_) {}

  Status AddNeighbor(int block_id, int peer_rank) {
    int buf_id = nbrvec_.size();
    nbrvec_.push_back({block_id, peer_rank, buf_id});
    return Status::OK;
  }

  void Print() {
    std::string nbrstr;
    for (auto nbr : nbrvec_) {
      nbrstr += std::to_string(nbr.block_id) + "/" +
                std::to_string(nbr.peer_rank) + ",";
    }

    logf(LOG_DBUG, "Rank %d, Block ID %d, Neighbors: %s", Globals::my_rank,
         block_id_, nbrstr.c_str());
  }

  Status AllocateBoundaryVariables(int bufsz) {
    logf(LOG_DBUG, "Allocating boundary variables");
    pbval_ = std::make_unique<BoundaryVariable>(shared_from_this());
    pbval_->SetupPersistentMPI(bufsz);
    logf(LOG_DBUG, "Allocating boundary variables - DONE!");
    return Status::OK;
  }

  void StartReceiving() { pbval_->StartReceiving(); }

  void SendBoundaryBuffers() { pbval_->SendBoundaryBuffers(); }

  void ReceiveBoundaryBuffers() { pbval_->ReceiveBoundaryBuffers(); }

  void ReceiveBoundaryBuffersWithWait() {
    pbval_->ReceiveBoundaryBuffersWithWait();
  }

  void ClearBoundary() { pbval_->ClearBoundary(); }

  uint64_t BytesSent() const { return pbval_->bytes_sent_; }

  uint64_t BytesRcvd() const { return pbval_->bytes_rcvd_; }

 private:
  friend class BoundaryVariable;
  int block_id_;
  std::unique_ptr<BoundaryVariable> pbval_;
  std::vector<NeighborBlock> nbrvec_;
};

class Mesh {
 public:
  Status AddBlock(std::shared_ptr<MeshBlock> block) {
    blocks_.push_back(block);
    return Status::OK;
  }

  Status AllocateBoundaryVariables(int bufsz) {
    for (auto b : blocks_) {
      Status s = b->AllocateBoundaryVariables(bufsz);
      if (!(s == Status::OK)) return s;
    }

    return Status::OK;
  }

  Status DoCommunicationRound() {
    logger_.LogBegin();

    logf(LOG_DBUG, "Start Receiving...");
    for (auto b : blocks_) {
      b->StartReceiving();
    }

    logf(LOG_DBUG, "Sending Boundary Buffers...");
    for (auto b : blocks_) {
      b->SendBoundaryBuffers();
    }

    logf(LOG_DBUG, "Receiving Boundary Buffers...");
    for (auto b : blocks_) {
      b->ReceiveBoundaryBuffers();
    }

    logf(LOG_DBUG, "Receiving Remaining Boundary Buffers...");
    for (auto b : blocks_) {
      b->ReceiveBoundaryBuffersWithWait();
    }

    logf(LOG_DBUG, "Clearing Boundaries...");
    for (auto b : blocks_) {
      b->ClearBoundary();
    }

    logger_.LogEnd();
    logger_.LogData(blocks_);

    return Status::OK;
  }

  Status DoCommunication(size_t nrounds) {
    Status s = Status::OK;
    for (size_t cnt = 0; cnt < nrounds; cnt++) {
      s = DoCommunicationRound();
      if (!(s == Status::OK)) break;
    }

    logger_.Aggregate();
    return s;
  }

  void Print() {
    for (auto block : blocks_) {
      block->Print();
    }
  }

 private:
  std::vector<std::shared_ptr<MeshBlock>> blocks_;
  Logger logger_;
};