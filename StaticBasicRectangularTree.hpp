/*
 * Copyright (C) 2017 Robert Mueller
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact: Robert Mueller <romueller@techfak.uni-bielefeld.de>
 * Faculty of Technology, Bielefeld University,
 * PO box 100131, DE-33501 Bielefeld, Germany
 */

#ifndef K2TREES_STATICBASICRECTANGULARTREE_HPP
#define K2TREES_STATICBASICRECTANGULARTREE_HPP

#include <queue>

#include "K2Tree.hpp"
#include "Utility.hpp"

/**
 * Basic rectangular implementation of K2Tree.
 *
 * Uses two different arities for rows and columns (kr, kc) on all levels.
 * The described relation matrix is rectangular with edge lengths of numRows and numCols,
 * where numRows (numCols) is the smallest power of kr (kc) that exceeds the row (column) numbers
 * of all relation pairs.
 */
template<typename E>
class KrKcTree : public virtual K2Tree<E> {

public:
    typedef E elem_type;

    typedef typename K2Tree<elem_type>::matrix_type matrix_type;
    typedef typename K2Tree<elem_type>::list_type list_type;
    typedef typename K2Tree<elem_type>::positions_type positions_type;
    typedef typename K2Tree<elem_type>::pairs_type pairs_type;


    KrKcTree() {
        // nothing to do
    }

    KrKcTree(const KrKcTree& other) {

        h_ = other.h_;
        kr_ = other.kr_;
        kc_ = other.kc_;
        numRows_ = other.numRows_;
        numCols_ = other.numCols_;
        null_ = other.null_;

        T_ = other.T_;
        L_ = other.L_;
        R_ = rank_type(&T_);

    }

    KrKcTree& operator=(const KrKcTree& other) {

        // check for self-assignment
        if (&other == this) {
            return *this;
        }

        h_ = other.h_;
        kr_ = other.kr_;
        kc_ = other.kc_;
        numRows_ = other.numRows_;
        numCols_ = other.numCols_;
        null_ = other.null_;

        T_ = other.T_;
        L_ = other.L_;
        R_ = rank_type(&T_);

        return *this;

    }

    /**
     * Matrix-based constructor (based on section 3.3.1. of Brisaboa et al.)
     *
     * Assumes that all rows of mat are equally long.
     */
    KrKcTree(const matrix_type& mat, const size_type kr, const size_type kc, const elem_type null = elem_type()) {

        null_ = null;

        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(mat.size(), kr_), logK(mat[0].size(), kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        std::vector<std::vector<bool>> levels(h_ - 1);
        buildFromMatrix(mat, levels, numRows_, numCols_, 1, 0, 0);

        size_type total = 0;
        for (auto l = 0; l < h_ - 1; l++) {
            total += levels[l].size();
        }
        T_ = bit_vector_type(total);

        bit_vector_type::iterator outIter = T_.begin();
        for (size_type l = 0; l < h_ - 1; l++) {

            outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
            levels[l].clear();
            levels[l].shrink_to_fit();

        }

        R_ = rank_type(&T_);

    }

    /**
     * Matrix-based constructor similar to the one above, but only a part of the relation matrix is used:
     *  x = first row of the submatrix
     *  y = first column of the submatrix
     *  nr = number of rows of the submatrix
     *  nc = number of columns of the submatrix
     */
    KrKcTree(const matrix_type& mat, const size_type x, const size_type y, const size_type nr, const size_type nc, const size_type kr, const size_type kc, const elem_type null = elem_type()) {

        null_ = null;

        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(nr, kr_), logK(nc, kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        checkParameters(nr, nc, kr, kc);

        std::vector<std::vector<bool>> levels(h_ - 1);
        buildFromMatrix(mat, levels, numRows_, numCols_, 1, x, y);

        size_type total = 0;
        for (auto l = 0; l < h_ - 1; l++) {
            total += levels[l].size();
        }
        T_ = bit_vector_type(total);

        bit_vector_type::iterator outIter = T_.begin();
        for (size_type l = 0; l < h_ - 1; l++) {

            outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
            levels[l].clear();
            levels[l].shrink_to_fit();

        }

        R_ = rank_type(&T_);

    }

    /**
     * List-of-lists-based constructor (based on sections 3.3.2. - 3.3.4. of Brisaboa et al.)
     *
     * The actually used method depends on parameter mode.
     */
    KrKcTree(const std::vector<list_type>& lists, const size_type kr, const size_type kc, const int mode, const elem_type null = elem_type()) {

        null_ = null;

        size_type maxCol = 0;
        for (auto& row : lists) {
            for (auto& elem : row) {
                maxCol = std::max(maxCol, elem.first);
            }
        }
        maxCol++; // for number of columns

        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(lists.size(), kr_), logK(maxCol, kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        switch (mode) {

            case 0: { // 3.3.2.

                std::vector<std::vector<bool>> levels(h_- 1);
                std::vector<typename list_type::const_iterator> cursors;
                for (auto iter = lists.begin(); iter != lists.end(); iter++) {
                    cursors.push_back(iter->begin());
                }

                buildFromLists(lists, cursors, levels, numRows_, numCols_, 1, 0, 0);

                size_type total = 0;
                for (auto l = 0; l < h_ - 1; l++) {
                    total += levels[l].size();
                }
                T_ = bit_vector_type(total);

                bit_vector_type::iterator outIter = T_.begin();
                for (size_type l = 0; l < h_ - 1; l++) {

                    outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
                    levels[l].clear();
                    levels[l].shrink_to_fit();

                }

                R_ = rank_type(&T_);

                break;

            }

            case 1: { // 3.3.3.

                buildFromListsViaTree(lists);

                R_ = rank_type(&T_);

                break;

            }

            case 2: { // 3.3.4.

                buildFromListsDynamicBitmaps(lists);

                break;

            }

            default: {

                buildFromListsDynamicBitmaps(lists);
                break;

            }

        }

    }

    /**
     * List-of-lists-based constructor similar to the one above, but only a part of the relation matrix is used:
     *  x = first row of the submatrix
     *  y = first column of the submatrix
     *  nr = number of rows of the submatrix
     *  nc = number of columns of the submatrix
     */
    KrKcTree(const std::vector<list_type>& lists, const size_type x, const size_type y, const size_type nr, const size_type nc, const size_type kr, const size_type kc, const int mode, const elem_type null = elem_type()) {

        null_ = null;

        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(nr, kr_), logK(nc, kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        checkParameters(nr, nc, kr, kc);

        switch (mode) {

            case 0: { // 3.3.2.

                std::vector<std::vector<bool>> levels(h_- 1);
                std::vector<typename list_type::const_iterator> cursors;
                for (auto iter = lists.begin(); iter != lists.end(); iter++) {

                    cursors.push_back(iter->begin());
                    auto& rowIter = cursors.back();
                    while ((rowIter != iter->end()) && (rowIter->first < y)) {
                        rowIter++;
                    }

                }

                buildFromLists(lists, cursors, levels, numRows_, numCols_, 1, x, y);

                size_type total = 0;
                for (auto l = 0; l < h_ - 1; l++) {
                    total += levels[l].size();
                }
                T_ = bit_vector_type(total);

                bit_vector_type::iterator outIter = T_.begin();
                for (size_type l = 0; l < h_ - 1; l++) {

                    outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
                    levels[l].clear();
                    levels[l].shrink_to_fit();

                }

                R_ = rank_type(&T_);

                break;

            }

            case 1: { // 3.3.3.

                buildFromListsViaTree(lists, x, y, nr, nc);

                R_ = rank_type(&T_);

                break;

            }

            case 2: { // 3.3.4.

                buildFromListsDynamicBitmaps(lists, x, y, nr, nc);

                break;

            }

            default: {

                buildFromListsDynamicBitmaps(lists, x, y, nr, nc);
                break;

            }

        }

    }

    /**
     * List-of-pairs-based constructor (based on section 3.3.5. of Brisaboa et al.)
     */
    KrKcTree(pairs_type& pairs, const size_type kr, const size_type kc, const elem_type null = elem_type()) {

        null_ = null;

        size_type maxRow = 0;
        size_type maxCol = 0;
        for (auto p : pairs) {
            maxRow = std::max(maxRow, p.row);
            maxCol = std::max(maxCol, p.col);
        }


        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(maxRow + 1, kr_), logK(maxCol + 1, kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        if (pairs.size() != 0) {
            buildFromListsInplace(pairs);
        }

        R_ = rank_type(&T_);

    }

    /**
     * List-of-pairs-based constructor similar to the one above, but only a part of the relation matrix is used:
     *  x = first row of the submatrix
     *  y = first column of the submatrix
     *  nr = number of rows of the submatrix
     *  nc = number of columns of the submatrix
     */
    KrKcTree(pairs_type& pairs, const size_type x, const size_type y, const size_type nr, const size_type nc, const size_type l, const size_type r, const size_type kr, const size_type kc, const elem_type null = elem_type()) {

        null_ = null;

        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(nr, kr_), logK(nc, kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        checkParameters(nr, nc, kr, kc);

        if (l != r) {
            buildFromListsInplace(pairs, x, y, nr, nc, l, r);
        }

        R_ = rank_type(&T_);

    }


    // returns the height of the K2Tree
    size_type getH() {
        return h_;
    }

    // returns the row arity of the K2Tree
    size_type getKr() {
        return kr_;
    }

    // returns the column arity of the K2Tree
    size_type getKc() {
        return kc_;
    }

    size_type getNumRows() override {
        return numRows_;
    }

    size_type getNumCols() override {
        return numCols_;
    }

    elem_type getNull() override {
        return null_;
    }


    bool isNotNull(size_type i, size_type j) override {
        return checkInit(i, j);
    }

    elem_type getElement(size_type i, size_type j) override {
        return getInit(i, j);
    }

    std::vector<elem_type> getSuccessorElements(size_type i) override {

        std::vector<elem_type> succs;
        allSuccessorElementsIterative(succs, i);
//        successorsElemInit(succs, i);

        return succs;

    }

    std::vector<size_type> getSuccessorPositions(size_type i) override {

        std::vector<size_type> succs;
//        successorsPosInit(succs, i);
        allSuccessorPositionsIterative(succs, i);

        return succs;

    }

    pairs_type getSuccessorValuedPositions(size_type i) override {

        pairs_type succs;
        allSuccessorValuedPositionsIterative(succs, i);
//        successorsValPosInit(succs, i);

        return succs;

    }

    std::vector<elem_type> getPredecessorElements(size_type j) override {

        std::vector<elem_type> preds;
        predecessorsElemInit(preds, j);

        return preds;

    }

    std::vector<size_type> getPredecessorPositions(size_type j) override {

        std::vector<size_type> preds;
        predecessorsPosInit(preds, j);

        return preds;

    }

    pairs_type getPredecessorValuedPositions(size_type j) override {

        pairs_type preds;
        predecessorsValPosInit(preds, j);

        return preds;

    }

    std::vector<elem_type> getElementsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {

        std::vector<elem_type> elements;
        rangeElemInit(elements, i1, i2, j1, j2);
//        rangeElemInit(elements, i1, std::min(i2, numRows_ - 1), j1, std::min(j2, numCols_ - 1));

        return elements;

    }

    positions_type getPositionsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {

        positions_type pairs;
        rangePosInit(pairs, i1, i2, j1, j2);
//        rangePosInit(pairs, i1, std::min(i2, numRows_ - 1), j1, std::min(j2, numCols_ - 1));

        return pairs;

    }

    pairs_type getValuedPositionsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {

        pairs_type pairs;
        rangeValPosInit(pairs, i1, i2, j1, j2);
//        rangeValPosInit(pairs, i1, std::min(i2, numRows_ - 1), j1, std::min(j2, numCols_ - 1));

        return pairs;

    }

    std::vector<elem_type> getAllElements() override {
        return getElementsInRange(0, numRows_ - 1, 0, numCols_ - 1);
    }

    positions_type getAllPositions() override {
        return getPositionsInRange(0, numRows_ - 1, 0, numCols_ - 1);
    }

    pairs_type getAllValuedPositions() override {
        return getValuedPositionsInRange(0, numRows_ - 1, 0, numCols_ - 1);
    }

    bool containsElement(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return elemInRangeInit(i1, i2, j1, j2);
//        return elemInRangeInit(i1, std::min(i2, numRows_ - 1), j1, std::min(j2, numCols_ - 1));
    }

    size_type countElements() override {

        size_type cnt = 0;
        for (size_type i = 0; i < L_.size(); i++) {
            cnt += (L_[i] != null_);
        }

        return cnt;

    }


    KrKcTree* clone() const override {
        return new KrKcTree<elem_type>(*this);
    }

    void print(bool all = false) override {

        std::cout << "### Parameters ###" << std::endl;
        std::cout << "h  = " << h_ << std::endl;
        std::cout << "kr  = " << kr_ << std::endl;
        std::cout << "kc  = " << kc_ << std::endl;
        std::cout << "numRows = " << numRows_ << std::endl;
        std::cout << "numCols = " << numCols_ << std::endl;
        std::cout << "null = " << null_ << std::endl;

        if (all) {

            std::cout << "### T ###" << std::endl;
            for (auto i = 0; i < T_.size(); i++) std::cout << T_[i];
            std::cout << std::endl << std::endl;

            std::cout << "### L ###" << std::endl;
            for (auto i = 0; i < L_.size(); i++) std::cout << L_[i];
            std::cout << std::endl << std::endl;

            std::cout << "### R ###" << std::endl;
            printRanks(R_);
            std::cout << std::endl;

        }

    }

    // note: can "invalidate" the data structure (containsLink() probably won't work correctly afterwards)
    void setNull(size_type i, size_type j) override {
        setInit(i, j);
    }

    size_type getFirstSuccessor(size_type i) override {
//        return firstSuccessorInit(i);
        return firstSuccessorPositionIterative(i);
    }


    /*
     * Method aliases using "relation nomenclature" (similar to the names proposed by Brisaboa et al.)
     */

    bool areRelated(size_type i, size_type j) override {
        return isNotNull(i, j);
    }

    std::vector<size_type> getSuccessors(size_type i) override {
        return getSuccessorPositions(i);
    }

    std::vector<size_type> getPredecessors(size_type j) override {
        return getPredecessorPositions(j);
    }

    positions_type getRange(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return getPositionsInRange(i1, i2, j1, j2);
    }

    bool containsLink(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return containsElement(i1, i2, j1, j2);
    }

    size_type countLinks() override {
        return countElements();
    }



private:
    // representation of all but the last levels of the K2Tree (internal structure)
    bit_vector_type T_;

    // representation of the last level of the K2Tree (actual values of the relation)
    std::vector<elem_type> L_;

    // rank data structure for navigation in T_
    rank_type R_;

    size_type h_; // height of the K2Tree
    size_type kr_; // row arity of the K2Tree
    size_type kc_; // column arity of the K2Tree
    size_type numRows_; // number of rows in the represented relation matrix
    size_type numCols_; // number of columns in the represented relation matrix

    elem_type null_; // null element


    /* helper method to check the feasibility of the tree parameters prior to construction */

    void checkParameters(const size_type nr, const size_type nc, const size_type kr, const size_type kc) {

        if ((numRows_ != nr) || (numCols_ != nc)) {

            std::string err = std::string() +
                              "Unsuitable parameters! " +
                              "The numbers of rows (nr) and columns (nc) have to be powers of kr resp. kc (using the same exponent h)." +
                              "But you gave me: nr = " + std::to_string(nr) + ", nc = " = std::to_string(nc) +
                              ", kr = " + std::to_string(kr) + " and kc = " + std::to_string(kc) + " leading to h = " + std::to_string(h_) +
                              " and " + std::to_string(numRows_) + " rows resp. " + std::to_string(numCols_) + " columns."
            ;

            throw std::runtime_error(err);

        }

    }

    /* helper method for construction from relation matrix */

    bool buildFromMatrix(const matrix_type& mat, std::vector<std::vector<bool>>& levels, size_type numRows, size_type numCols, size_type l, size_type p, size_type q) {// 3.3.1 / Algorithm 1

        if (l == h_) {

            std::vector<elem_type> C;

            for (auto i = 0; i < kr_; i++) {
                for (auto j = 0; j < kc_; j++) {
                    C.push_back((((p + i) < mat.size()) && ((q + j) < mat[0].size())) ? mat[p + i][q + j] : null_);
                }
            }

            if (isAll(C, null_)) {
                return false;
            } else {

                L_.insert(L_.end(), C.begin(), C.end());
                return true;

            }

        } else {

            std::vector<bool> C;

            for (auto i = 0; i < kr_; i++) {
                for (auto j = 0; j < kc_; j++) {
                    C.push_back(buildFromMatrix(mat, levels, numRows / kr_, numCols / kc_, l + 1, p + i * (numRows / kr_), q + j * (numCols / kc_)));
                }
            }

            if (isAllZero(C)) {
                return false;
            } else {

                levels[l - 1].insert(levels[l - 1].end(), C.begin(), C.end());
                return true;

            }

        }

    }

    /* helper method for construction from relation lists */

    bool buildFromLists(const std::vector<list_type>& lists, std::vector<typename list_type::const_iterator>& cursors, std::vector<std::vector<bool>>& levels, size_type numRows, size_type numCols, size_type l, size_type p, size_type q) {// 3.3.2

        if (l == h_) {

            std::vector<elem_type> C;

            for (auto i = 0; i < kr_; i++) {
                for (auto j = 0; j < kc_; j++) {

                    C.push_back(((p + i) < lists.size()) && (cursors[p + i] != lists[p + i].end()) && ((q + j) == cursors[p + i]->first) ? cursors[p + i]->second : null_);
                    if (C.back()) cursors[p + i]++;

                }
            }

            if (isAll(C, null_)) {
                return false;
            } else {

                L_.insert(L_.end(), C.begin(), C.end());
                return true;

            }

        } else {

            std::vector<bool> C;

            for (auto i = 0; i < kr_; i++) {
                for (auto j = 0; j < kc_; j++) {
                    C.push_back(buildFromLists(lists, cursors, levels, numRows / kr_, numCols / kc_, l + 1, p + i * (numRows / kr_), q + j * (numCols / kc_)));
                }
            }

            if (isAllZero(C)) {
                return false;
            } else {

                levels[l - 1].insert(levels[l - 1].end(), C.begin(), C.end());
                return true;

            }

        }

    }

    /* helper methods for construction from relation lists via temporary tree */

    void buildFromListsViaTree(const std::vector<list_type>& lists) {// 3.3.3, so far without special bit vectors without initialisation

        Node<elem_type>* root = new Node<elem_type>(null_);

        for (size_type i = 0; i < lists.size(); i++) {
            for (size_type j = 0; j < lists[i].size(); j++) {
                insert(root, numRows_, numCols_, i, lists[i][j].first, lists[i][j].second);
            }
        }

        if (!root->isLeaf()) {

            std::vector<bool> T;

            // traverse over tree and generate T and L while doing it
            std::queue<Node<elem_type>*> queue;
            Node<elem_type>* node;
            Node<elem_type>* child;
            queue.push(root);

            while (!queue.empty()) {

                node = queue.front();
                queue.pop();

                for (size_type i = 0; i < kr_ * kc_; i++) {

                    child = node->getChild(i);

                    if (child != 0 && child->isLeaf()) {
                        L_.push_back(child->getLabel());
                    } else {

                        T.push_back(child != 0);

                        if (T.back()) {
                            queue.push(child);
                        }

                    }

                }

            }

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        delete root;

    }

    void buildFromListsViaTree(const std::vector<list_type>& lists, size_type x, size_type y, size_type nr, size_type nc) {// 3.3.3, so far without special bit vectors without initialisation

        Node<elem_type>* root = new Node<elem_type>(null_);

        for (size_type i = x; (i < x + nr) && (i < lists.size()); i++) {
            for (size_type j = 0; j < lists[i].size(); j++) {
                if ((y <= lists[i][j].first) && (lists[i][j].first < (y + nc))) {
                    insert(root, nr, nc, i - x, lists[i][j].first - y, lists[i][j].second);
                }
            }
        }

        if (!root->isLeaf()) {

            std::vector<bool> T;

            // traverse over tree and generate T and L while doing it
            std::queue<Node<elem_type>*> queue;
            Node<elem_type>* node;
            Node<elem_type>* child;
            queue.push(root);

            while (!queue.empty()) {

                node = queue.front();
                queue.pop();

                for (size_type i = 0; i < kr_ * kc_; i++) {

                    child = node->getChild(i);

                    if (child != 0 && child->isLeaf()) {
                        L_.push_back(child->getLabel());
                    } else {

                        T.push_back(child != 0);

                        if (T.back()) {
                            queue.push(child);
                        }

                    }

                }

            }

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        delete root;

    }

    void insert(Node<elem_type>* node, size_type numRows, size_type numCols, size_type p, size_type q, elem_type val) {

        if (numRows == kr_) { // alternatively, numCols == kc_; occurs (currently) at the same time since we use the same height in both dimensions

            if (node->isLeaf()) {
                node->turnInternal(kr_ * kc_, true);
            }

            node->addChild(p * kc_ + q, val);

        } else {

            if (node->isLeaf()) {
                node->turnInternal(kr_ * kc_, false);
            }

            size_type z = (p / (numRows / kr_)) * kc_ + q / (numCols / kc_);

            insert(node->hasChild(z) ? node->getChild(z) : node->addChild(z, null_), numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_), val);

        }

    }

    /* helper methods for construction from relation lists via dynamic bitmap representations */

    void buildFromListsDynamicBitmaps(const std::vector<list_type>& lists) {// 3.3.4, currently no succinct dynamic bitmaps

        if (h_ == 1) {

            L_ = std::vector<elem_type>(kr_ * kc_, null_);

            for (size_type i = 0; i < lists.size(); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    L_[i * kc_ + lists[i][j].first] = lists[i][j].second;
                }
            }

            if (isAll(L_, null_)) {
                L_ = std::vector<elem_type>(0);
            }

        } else {

            std::vector<bool> T;
            NaiveDynamicRank R;

            for (size_type i = 0; i < lists.size(); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    insertInit(T, R, i, lists[i][j].first, lists[i][j].second);
                }
            }

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        R_ = rank_type(&T_);

    }

    void buildFromListsDynamicBitmaps(const std::vector<list_type>& lists, size_type x, size_type y, size_type nr, size_type nc) {// 3.3.4, currently no succinct dynamic bitmaps

        if (h_ == 1) {

            L_ = std::vector<elem_type>(kr_ * kc_, null_);

            for (size_type i = x; (i < x + nr) && (i < lists.size()); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    if ((y <= lists[i][j].first) && (lists[i][j].first < (y + nc))) {
                        L_[(i - x) * kc_ + lists[i][j].first - y] = lists[i][j].second;
                    }
                }
            }

            if (isAll(L_, null_)) {
                L_ = std::vector<elem_type>(0);
            }

        } else {

            std::vector<bool> T;
            NaiveDynamicRank R;

            for (size_type i = x; (i < x + nr) && (i < lists.size()); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    if ((y <= lists[i][j].first) && (lists[i][j].first < (y + nc))) {
                        insertInit(T, R, i - x, lists[i][j].first - y, lists[i][j].second);
                    }
                }
            }

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        R_ = rank_type(&T_);

    }

    void insertInit(std::vector<bool>& T, NaiveDynamicRank& R, size_type p, size_type q, elem_type val) {

        if (T.empty()) {

            T = std::vector<bool>(kr_ * kc_);
            R = NaiveDynamicRank(T);

        }

        insert(T, R, numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), q % (numCols_ / kc_), val, (p / (numRows_ / kr_)) * kc_ + q / (numCols_ / kc_), 1);

    }

    void insert(std::vector<bool>& T, NaiveDynamicRank& R, size_type numRows, size_type numCols, size_type p, size_type q, elem_type val, size_type z, size_type l) {

        if (!T[z]) {

            T[z] = 1;
            R.increaseFrom(z + 1);

            size_type y = R.rank(z + 1) * kr_ * kc_ + (p / (numRows / kr_)) * kc_ + q / (numCols / kc_);

            if ((l + 1) == h_) {

                L_.insert(L_.begin() + R.rank(z + 1) * kr_ * kc_ - T.size(), kr_ * kc_, null_);
                L_[y - T.size()] = 1;

            } else {

                T.insert(T.begin() + R.rank(z + 1) * kr_ * kc_, kr_ * kc_, 0);
                R.insert(R.rank(z + 1) * kr_ * kc_ + 1, kr_ * kc_);

                insert(T, R, numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_), val, y, l + 1);

            }

        } else {

            size_type y = R.rank(z + 1) * kr_ * kc_ + (p / (numRows / kr_)) * kc_ + q / (numCols / kc_);

            if ((l + 1) == h_) {
                L_[y - T.size()] = 1;
            } else {
                insert(T, R, numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_), val, y, l + 1);
            }

        }

    }

    /* helper methods for inplace construction from single list of pairs */

    size_type computeKey(const typename pairs_type::value_type& pair, const Subproblem& sp, size_type widthRow, size_type widthCol) {
        return ((pair.row - sp.firstRow) / widthRow) * kc_ + (pair.col - sp.firstCol) / widthCol;
    }

    void countingSort(pairs_type& pairs, std::vector<std::pair<size_type, size_type>>& intervals, const Subproblem& sp, size_type widthRow, size_type widthCol, size_type sup) {

        std::vector<size_type> counts(sup);

        // determine key frequencies
        for (auto i = sp.left; i < sp.right; i++) {
            counts[computeKey(pairs[i], sp, widthRow, widthCol)]++;
        }

        // determine starting index for each key
        size_type total = 0;
        size_type tmp;

        for (auto key = 0; key < sup; key++) {

            tmp = counts[key];
            counts[key] = total;
            total += tmp;

            intervals[key].first = counts[key];
            intervals[key].second = total;

        }

        // reorder pairs of current subproblem
        pairs_type tmpPairs(sp.right - sp.left + 1);
        for (auto i = sp.left; i < sp.right; i++) {

            tmpPairs[counts[computeKey(pairs[i], sp, widthRow, widthCol)]] = pairs[i];
            counts[computeKey(pairs[i], sp, widthRow, widthCol)]++;

        }

        for (auto i = sp.left; i < sp.right; i++) {
            pairs[i] = tmpPairs[i - sp.left];
        }

    }

    void buildFromListsInplace(pairs_type& pairs) {// 3.3.5

        std::queue<Subproblem> queue;
        Subproblem sp;
        size_type Sr, Sc;
        std::vector<std::pair<size_type, size_type>> intervals(kr_ * kc_);
        std::vector<bool> T;
        std::vector<elem_type> appToL;

        queue.push(Subproblem(0, numRows_ - 1, 0, numCols_ - 1, 0, pairs.size()));

        while (!queue.empty()) {

            sp = queue.front();
            queue.pop();

            Sr = sp.lastRow - sp.firstRow + 1;
            Sc = sp.lastCol - sp.firstCol + 1;

            if (Sr > kr_) {

                countingSort(pairs, intervals, sp, Sr / kr_, Sc / kc_, kr_ * kc_);

                for (size_type i = 0; i < kr_ * kc_; i++) {

                    if (intervals[i].first < intervals[i].second) {

                        T.push_back(true);
                        queue.push(Subproblem(
                                sp.firstRow + (i / kc_) * (Sr / kr_),
                                sp.firstRow + (i / kc_ + 1) * (Sr / kr_) - 1,
                                sp.firstCol + (i % kc_) * (Sc / kc_),
                                sp.firstCol + (i % kc_ + 1) * (Sc / kc_) - 1,
                                sp.left + intervals[i].first,
                                sp.left + intervals[i].second
                        ));

                    } else {
                        T.push_back(false);
                    }

                }

            } else {

                appToL = std::vector<elem_type>(kr_ * kc_);

                for (size_type i = sp.left; i < sp.right; i++) {
                    appToL[(pairs[i].row - sp.firstRow) * kc_ + (pairs[i].col - sp.firstCol)] = pairs[i].val;
                }

                L_.insert(L_.end(), appToL.begin(), appToL.end());

            }

        }

        T_ = bit_vector_type(T.size());
        std::move(T.begin(), T.end(), T_.begin());

    }

    void buildFromListsInplace(pairs_type& pairs, size_type x, size_type y, size_type nr, size_type nc, size_type l, size_type r) {// 3.3.5

        std::queue<Subproblem> queue;
        Subproblem sp;
        size_type Sr, Sc;
        std::vector<std::pair<size_type, size_type>> intervals(kr_ * kc_);
        std::vector<bool> T;
        std::vector<elem_type> appToL;

        queue.push(Subproblem(x, x + nr - 1, y, y + nc - 1, l, r));

        while (!queue.empty()) {

            sp = queue.front();
            queue.pop();

            Sr = sp.lastRow - sp.firstRow + 1;
            Sc = sp.lastCol - sp.firstCol + 1;

            if (Sr > kr_) {

                countingSort(pairs, intervals, sp, Sr / kr_, Sc / kc_, kr_ * kc_);

                for (size_type i = 0; i < kr_ * kc_; i++) {

                    if (intervals[i].first < intervals[i].second) {

                        T.push_back(true);
                        queue.push(Subproblem(
                                sp.firstRow + (i / kc_) * (Sr / kr_),
                                sp.firstRow + (i / kc_ + 1) * (Sr / kr_) - 1,
                                sp.firstCol + (i % kc_) * (Sc / kc_),
                                sp.firstCol + (i % kc_ + 1) * (Sc / kc_) - 1,
                                sp.left + intervals[i].first,
                                sp.left + intervals[i].second
                        ));

                    } else {
                        T.push_back(false);
                    }

                }

            } else {

                appToL = std::vector<elem_type>(kr_ * kc_);

                for (size_type i = sp.left; i < sp.right; i++) {
                    appToL[(pairs[i].row - sp.firstRow) * kc_ + (pairs[i].col - sp.firstCol)] = pairs[i].val;
                }

                L_.insert(L_.end(), appToL.begin(), appToL.end());

            }

        }

        T_ = bit_vector_type(T.size());
        std::move(T.begin(), T.end(), T_.begin());

    }



    /* isNotNull() */

    bool checkInit(size_type p, size_type q) {
        return (L_.empty()) ? false : check(numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), q % (numCols_ / kc_), (p / (numRows_ / kr_)) * kc_ + q / (numCols_ / kc_));
    }

    bool check(size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {
            return (L_[z - T_.size()] != null_);
        } else {
            return T_[z] ? check(numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_), R_.rank(z + 1) * kr_ * kc_ + (p / (numRows / kr_)) * kc_ + q / (numCols / kc_)) : false;
        }

    }

    /* getElement() */

    elem_type getInit(size_type p, size_type q) {
        return (L_.empty()) ? null_ : get(numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), q % (numCols_ / kc_), (p / (numRows_ / kr_)) * kc_ + q / (numCols_ / kc_));
    }

    elem_type get(size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {
            return L_[z - T_.size()];
        } else {
            return T_[z] ? get(numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_), R_.rank(z + 1) * kr_ * kc_ + (p / (numRows / kr_)) * kc_ + q / (numCols / kc_)) : null_;
        }

    }

    /* getSuccessorElements() */

    void allSuccessorElementsIterative(std::vector<elem_type>& succs, size_type p) {

        if (L_.empty()) return;

        std::queue<SubrowInfo> queue, nextLevelQueue;
        size_type lenT = T_.size();

        if (lenT == 0) {

            size_type offset = p * numCols_;
            for (size_type i = 0; i < numCols_; i++) {
                if (L_[offset + i] != null_) {
                    succs.push_back(L_[offset + i]);
                }
            }

        } else {

            // successorsPosInit
            size_type nr = numRows_/ kr_;
            size_type nc = numCols_/ kc_;
            size_type relP = p;
            for (size_type j = 0, dq = 0, z = kc_ * (relP / nr); j < kc_; j++, dq += nc, z++) {
                queue.push(SubrowInfo(dq, z));
            }

            // successorsPos
            relP %= nr;
            nr /= kr_;
            nc /= kc_;
            for (; nr > 1; relP %= nr, nr /= kr_, nc /= kc_) {

                while (!queue.empty()) {

                    auto& cur = queue.front();

                    if (T_[cur.z]) {

                        auto y = R_.rank(cur.z + 1) * kr_ * kc_ + kc_ * (relP / nr);

                        for (size_type j = 0, newDq = cur.dq; j < kc_; j++, newDq += nc, y++) {
                            nextLevelQueue.push(SubrowInfo(newDq, y));
                        }

                    }

                    queue.pop();

                }

                queue.swap(nextLevelQueue);

            }


            while (!queue.empty()) {

                auto& cur = queue.front();

                if (T_[cur.z]) {

                    auto y = R_.rank(cur.z + 1) * kr_ * kc_ + kc_ * (relP / nr) - lenT;

                    for (size_type j = 0, newDq = cur.dq; j < kc_; j++, newDq += nc, y++) {
                        if (L_[y] != null_) {
                            succs.push_back(L_[y]);
                        }
                    }

                }

                queue.pop();

            }

        }

    }

    void successorsElemInit(std::vector<elem_type>& succs, size_type p) {

        if (!L_.empty()) {

            size_type y = kc_ * (p / (numRows_ / kr_));

            for (size_type j = 0; j < kc_; j++) {
                successorsElem(succs, numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), (numCols_ / kc_) * j, y + j);
            }

        }

    }

    void successorsElem(std::vector<elem_type>& succs, size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                succs.push_back(L_[z - T_.size()]);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * kr_ * kc_ + kc_ * (p / (numRows / kr_));

                for (size_type j = 0; j < kc_; j++) {
                    successorsElem(succs, numRows / kr_, numCols / kc_, p % (numRows / kr_), q + (numCols / kc_) * j, y + j);
                }

            }

        }


    }

    /* getSuccessorPositions() */

    void allSuccessorPositionsIterative(std::vector<size_type>& succs, size_type p) {

        if (L_.empty()) return;

        std::queue<SubrowInfo> queue, nextLevelQueue;
        size_type lenT = T_.size();

        if (lenT == 0) {

            size_type offset = p * numCols_;
            for (size_type i = 0; i < numCols_; i++) {
                if (L_[offset + i] != null_) {
                    succs.push_back(i);
                }
            }

        } else {

            // successorsPosInit
            size_type nr = numRows_/ kr_;
            size_type nc = numCols_/ kc_;
            size_type relP = p;
            for (size_type j = 0, dq = 0, z = kc_ * (relP / nr); j < kc_; j++, dq += nc, z++) {
                queue.push(SubrowInfo(dq, z));
            }

            // successorsPos
            relP %= nr;
            nr /= kr_;
            nc /= kc_;
            for (; nr > 1; relP %= nr, nr /= kr_, nc /= kc_) {

                while (!queue.empty()) {

                    auto& cur = queue.front();

                    if (T_[cur.z]) {

                        auto y = R_.rank(cur.z + 1) * kr_ * kc_ + kc_ * (relP / nr);

                        for (size_type j = 0, newDq = cur.dq; j < kc_; j++, newDq += nc, y++) {
                            nextLevelQueue.push(SubrowInfo(newDq, y));
                        }

                    }

                    queue.pop();

                }

                queue.swap(nextLevelQueue);

            }


            while (!queue.empty()) {

                auto& cur = queue.front();

                if (T_[cur.z]) {

                    auto y = R_.rank(cur.z + 1) * kr_ * kc_ + kc_ * (relP / nr) - lenT;

                    for (size_type j = 0, newDq = cur.dq; j < kc_; j++, newDq += nc, y++) {
                        if (L_[y] != null_) {
                            succs.push_back(newDq);
                        }
                    }

                }

                queue.pop();

            }

        }

    }

    void successorsPosInit(std::vector<size_type>& succs, size_type p) {

        if (!L_.empty()) {

            size_type y = kc_ * (p / (numRows_ / kr_));

            for (size_type j = 0; j < kc_; j++) {
                successorsPos(succs, numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), (numCols_ / kc_) * j, y + j);
            }

        }

    }

    void successorsPos(std::vector<size_type>& succs, size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                succs.push_back(q);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * kr_ * kc_ + kc_ * (p / (numRows / kr_));

                for (size_type j = 0; j < kc_; j++) {
                    successorsPos(succs, numRows / kr_, numCols / kc_, p % (numRows / kr_), q + (numCols / kc_) * j, y + j);
                }

            }

        }


    }

    /* getSuccessorValuedPositions() */

    void allSuccessorValuedPositionsIterative(pairs_type& succs, size_type p) {

        if (L_.empty()) return;

        std::queue<SubrowInfo> queue, nextLevelQueue;
        size_type lenT = T_.size();

        if (lenT == 0) {

            size_type offset = p * numCols_;
            for (size_type i = 0; i < numCols_; i++) {
                if (L_[offset + i] != null_) {
                    succs.push_back(ValuedPosition<elem_type>(p, i, L_[offset + i]));
                }
            }

        } else {

            // successorsPosInit
            size_type nr = numRows_/ kr_;
            size_type nc = numCols_/ kc_;
            size_type relP = p;
            for (size_type j = 0, dq = 0, z = kc_ * (relP / nr); j < kc_; j++, dq += nc, z++) {
                queue.push(SubrowInfo(dq, z));
            }

            // successorsPos
            relP %= nr;
            nr /= kr_;
            nc /= kc_;
            for (; nr > 1; relP %= nr, nr /= kr_, nc /= kc_) {

                while (!queue.empty()) {

                    auto& cur = queue.front();

                    if (T_[cur.z]) {

                        auto y = R_.rank(cur.z + 1) * kr_ * kc_ + kc_ * (relP / nr);

                        for (size_type j = 0, newDq = cur.dq; j < kc_; j++, newDq += nc, y++) {
                            nextLevelQueue.push(SubrowInfo(newDq, y));
                        }

                    }

                    queue.pop();

                }

                queue.swap(nextLevelQueue);

            }


            while (!queue.empty()) {

                auto& cur = queue.front();

                if (T_[cur.z]) {

                    auto y = R_.rank(cur.z + 1) * kr_ * kc_ + kc_ * (relP / nr) - lenT;

                    for (size_type j = 0, newDq = cur.dq; j < kc_; j++, newDq += nc, y++) {
                        if (L_[y] != null_) {
                            succs.push_back(ValuedPosition<elem_type>(p, newDq, L_[y]));
                        }
                    }

                }

                queue.pop();

            }

        }

    }

    void successorsValPosInit(pairs_type& succs, size_type p) {

        if (!L_.empty()) {

            size_type y = kc_ * (p / (numRows_ / kr_));

            for (size_type j = 0; j < kc_; j++) {
                successorsValPos(succs, numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), (numCols_ / kc_) * j, y + j);
            }

            for (auto& s : succs) {
                s.row = p;
            }

        }

    }

    void successorsValPos(pairs_type& succs, size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                succs.push_back(ValuedPosition<elem_type>(0, q, L_[z - T_.size()]));
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * kr_ * kc_ + kc_ * (p / (numRows / kr_));

                for (size_type j = 0; j < kc_; j++) {
                    successorsValPos(succs, numRows / kr_, numCols / kc_, p % (numRows / kr_), q + (numCols / kc_) * j, y + j);
                }

            }

        }


    }

    /* getFirstSuccessor() */

    size_type firstSuccessorPositionIterative(size_type p) {

        if (L_.empty()) return numCols_;

        if (T_.size() == 0) {

            size_type offset = p * numCols_;
            for (size_type i = 0; i < numCols_; i++) {
                if (L_[offset + i] != null_) {
                    return i;
                }
            }

        } else {

            std::stack<ExtendedSubrowInfo> stack;
            stack.emplace(numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), 0, kc_ * (p / (numRows_ / kr_)), 0);

            while (!stack.empty()) {

                auto& cur = stack.top();

                if (cur.j == kc_) {
                    stack.pop();
                } else {

                    if (cur.z >= T_.size()) {

                        if (L_[cur.z - T_.size()] != null_) {
                            return cur.dq;
                        }

                    } else {

                        if (T_[cur.z]) {
                            stack.emplace(cur.nr / kr_, cur.nc / kc_, cur.p % (cur.nr / kr_), cur.dq, R_.rank(cur.z + 1) * kr_ * kc_ + kc_ * (cur.p / (cur.nr / kr_)), 0);
                        }

                    }

                    cur.dq += cur.nc;
                    cur.z++;
                    cur.j++;

                }

            }
        }

        return numCols_;

    }

    size_type firstSuccessorInit(size_type p) {

        size_type pos = numCols_;

        if (!L_.empty()) {

            size_type y = kc_ * (p / (numRows_ / kr_));

            for (size_type j = 0; j < kc_ && pos == numCols_; j++) {
                pos = firstSuccessor(numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), (numCols_ / kc_) * j, y + j);
            }

        }

        return pos;

    }

    size_type firstSuccessor(size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        size_type pos = numCols_;

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                pos = q;
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * kr_ * kc_ + kc_ * (p / (numRows / kr_));

                for (size_type j = 0; j < kc_ && pos == numCols_; j++) {
                    pos = firstSuccessor(numRows / kr_, numCols / kc_, p % (numRows / kr_), q + (numCols / kc_) * j, y + j);
                }

            }

        }

        return pos;

    }

    /* getPredecessorElements() */

    void predecessorsElemInit(std::vector<elem_type>& preds, size_type q) {

        if (!L_.empty()) {

            size_type y = q / (numCols_ / kc_);

            for (size_type i = 0; i < kr_; i++) {
                predecessorsElem(preds, numRows_ / kr_, numCols_ / kc_, q % (numCols_ / kc_), (numRows_ / kr_) * i, y + i * kc_);
            }

        }

    }

    void predecessorsElem(std::vector<elem_type>& preds, size_type numRows, size_type numCols, size_type q, size_type p, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                preds.push_back(L_[z - T_.size()]);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * kr_ * kc_ + q / (numCols / kc_);

                for (size_type i = 0; i < kr_; i++) {
                    predecessorsElem(preds, numRows / kr_, numCols / kc_,  q % (numCols / kc_), p + (numRows / kr_) * i, y + i * kc_);
                }

            }

        }

    }

    /* getPredecessorPositions() */

    void predecessorsPosInit(std::vector<size_type>& preds, size_type q) {

        if (!L_.empty()) {

            size_type y = q / (numCols_ / kc_);

            for (size_type i = 0; i < kr_; i++) {
                predecessorsPos(preds, numRows_ / kr_, numCols_ / kc_, q % (numCols_ / kc_), (numRows_ / kr_) * i, y + i * kc_);
            }

        }

    }

    void predecessorsPos(std::vector<size_type>& preds, size_type numRows, size_type numCols, size_type q, size_type p, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                preds.push_back(p);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * kr_ * kc_ + q / (numCols / kc_);

                for (size_type i = 0; i < kr_; i++) {
                    predecessorsPos(preds, numRows / kr_, numCols / kc_,  q % (numCols / kc_), p + (numRows / kr_) * i, y + i * kc_);
                }

            }

        }

    }

    /* getPredecessorValuedPositions() */

    void predecessorsValPosInit(pairs_type& preds, size_type q) {

        if (!L_.empty()) {

            size_type y = q / (numCols_ / kc_);

            for (size_type i = 0; i < kr_; i++) {
                predecessorsValPos(preds, numRows_ / kr_, numCols_ / kc_, q % (numCols_ / kc_), (numRows_ / kr_) * i, y + i * kc_);
            }

            for (auto& p : preds) {
                p.col = q;
            }

        }

    }

    void predecessorsValPos(pairs_type& preds, size_type numRows, size_type numCols, size_type q, size_type p, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                preds.push_back(ValuedPosition<elem_type>(p, 0, L_[z - T_.size()]));
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * kr_ * kc_ + q / (numCols / kc_);

                for (size_type i = 0; i < kr_; i++) {
                    predecessorsValPos(preds, numRows / kr_, numCols / kc_,  q % (numCols / kc_), p + (numRows / kr_) * i, y + i * kc_);
                }

            }

        }

    }

    /* getElementsInRange() */

    void rangeElemInit(std::vector<elem_type>& elements, size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            size_type p1Prime, p2Prime;

            for (auto i = p1 / (numRows_ / kr_); i <= p2 / (numRows_ / kr_); i++) {

                p1Prime = (i == p1 / (numRows_ / kr_)) * (p1 % (numRows_ / kr_));
                p2Prime = (i == p2 / (numRows_ / kr_)) ? p2 % (numRows_ / kr_) : (numRows_ / kr_) - 1;

                for (auto j = q1 / (numCols_ / kc_); j <= q2 / (numCols_ / kc_); j++) {
                    rangeElem(
                            elements,
                            numRows_ / kr_,
                            numCols_ / kc_,
                            p1Prime,
                            p2Prime,
                            (j == q1 / (numCols_ / kc_)) * (q1 % (numCols_ / kc_)),
                            (j == q2 / (numCols_ / kc_)) ? q2 % (numCols_ / kc_) : (numCols_ / kc_) - 1,
                            (numRows_ / kr_) * i,
                            (numCols_ / kc_) * j,
                            kc_ * i + j
                    );
                }

            }

        }

    }

    void rangeElem(std::vector<elem_type>& elements, size_type numRows, size_type numCols, size_type p1, size_type p2, size_type q1, size_type q2, size_type dp, size_type dq, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                elements.push_back(L_[z - T_.size()]);
            }

        } else {

            if (T_[z]) {

                auto y = R_.rank(z + 1) * kr_ * kc_;
                size_type p1Prime, p2Prime;

                for (auto i = p1 / (numRows / kr_); i <= p2 / (numRows / kr_); i++) {

                    p1Prime = (i == p1 / (numRows / kr_)) * (p1 % (numRows / kr_));
                    p2Prime = (i == p2 / (numRows / kr_)) ? p2 % (numRows / kr_) : numRows / kr_ - 1;

                    for (auto j = q1 / (numCols / kc_); j <= q2 / (numCols / kc_); j++) {
                        rangeElem(
                                elements,
                                numRows / kr_,
                                numCols / kc_,
                                p1Prime,
                                p2Prime,
                                (j == q1 / (numCols / kc_)) * (q1 % (numCols / kc_)),
                                (j == q2 / (numCols / kc_)) ? q2 % (numCols / kc_) : numCols / kc_ - 1,
                                dp + (numRows / kr_) * i,
                                dq + (numCols / kc_) * j,
                                y + kc_ * i + j
                        );
                    }

                }

            }

        }

    }

    /* getPositionsInRange() */

    void rangePosInit(positions_type& pairs, size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            size_type p1Prime, p2Prime;

            for (auto i = p1 / (numRows_ / kr_); i <= p2 / (numRows_ / kr_); i++) {

                p1Prime = (i == p1 / (numRows_ / kr_)) * (p1 % (numRows_ / kr_));
                p2Prime = (i == p2 / (numRows_ / kr_)) ? p2 % (numRows_ / kr_) : (numRows_ / kr_) - 1;

                for (auto j = q1 / (numCols_ / kc_); j <= q2 / (numCols_ / kc_); j++) {
                    rangePos(
                            pairs,
                            numRows_ / kr_,
                            numCols_ / kc_,
                            p1Prime,
                            p2Prime,
                            (j == q1 / (numCols_ / kc_)) * (q1 % (numCols_ / kc_)),
                            (j == q2 / (numCols_ / kc_)) ? q2 % (numCols_ / kc_) : (numCols_ / kc_) - 1,
                            (numRows_ / kr_) * i,
                            (numCols_ / kc_) * j,
                            kc_ * i + j
                    );
                }

            }

        }

    }

    void rangePos(positions_type& pairs, size_type numRows, size_type numCols, size_type p1, size_type p2, size_type q1, size_type q2, size_type dp, size_type dq, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                pairs.push_back(std::make_pair(dp, dq));
            }

        } else {

            if (T_[z]) {

                auto y = R_.rank(z + 1) * kr_ * kc_;
                size_type p1Prime, p2Prime;

                for (auto i = p1 / (numRows / kr_); i <= p2 / (numRows / kr_); i++) {

                    p1Prime = (i == p1 / (numRows / kr_)) * (p1 % (numRows / kr_));
                    p2Prime = (i == p2 / (numRows / kr_)) ? p2 % (numRows / kr_) : numRows / kr_ - 1;

                    for (auto j = q1 / (numCols / kc_); j <= q2 / (numCols / kc_); j++) {
                        rangePos(
                                pairs,
                                numRows / kr_,
                                numCols / kc_,
                                p1Prime,
                                p2Prime,
                                (j == q1 / (numCols / kc_)) * (q1 % (numCols / kc_)),
                                (j == q2 / (numCols / kc_)) ? q2 % (numCols / kc_) : numCols / kc_ - 1,
                                dp + (numRows / kr_) * i,
                                dq + (numCols / kc_) * j,
                                y + kc_ * i + j
                        );
                    }

                }

            }

        }

    }

    /* getValuedPositionsInRange() */

    void rangeValPosInit(pairs_type& pairs, size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            size_type p1Prime, p2Prime;

            for (auto i = p1 / (numRows_ / kr_); i <= p2 / (numRows_ / kr_); i++) {

                p1Prime = (i == p1 / (numRows_ / kr_)) * (p1 % (numRows_ / kr_));
                p2Prime = (i == p2 / (numRows_ / kr_)) ? p2 % (numRows_ / kr_) : (numRows_ / kr_) - 1;

                for (auto j = q1 / (numCols_ / kc_); j <= q2 / (numCols_ / kc_); j++) {
                    rangeValPos(
                            pairs,
                            numRows_ / kr_,
                            numCols_ / kc_,
                            p1Prime,
                            p2Prime,
                            (j == q1 / (numCols_ / kc_)) * (q1 % (numCols_ / kc_)),
                            (j == q2 / (numCols_ / kc_)) ? q2 % (numCols_ / kc_) : (numCols_ / kc_) - 1,
                            (numRows_ / kr_) * i,
                            (numCols_ / kc_) * j,
                            kc_ * i + j
                    );
                }

            }

        }

    }

    void rangeValPos(pairs_type& pairs, size_type numRows, size_type numCols, size_type p1, size_type p2, size_type q1, size_type q2, size_type dp, size_type dq, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                pairs.push_back(ValuedPosition<elem_type>(dp, dq, L_[z - T_.size()]));
            }

        } else {

            if (T_[z]) {

                auto y = R_.rank(z + 1) * kr_ * kc_;
                size_type p1Prime, p2Prime;

                for (auto i = p1 / (numRows / kr_); i <= p2 / (numRows / kr_); i++) {

                    p1Prime = (i == p1 / (numRows / kr_)) * (p1 % (numRows / kr_));
                    p2Prime = (i == p2 / (numRows / kr_)) ? p2 % (numRows / kr_) : numRows / kr_ - 1;

                    for (auto j = q1 / (numCols / kc_); j <= q2 / (numCols / kc_); j++) {
                        rangeValPos(
                                pairs,
                                numRows / kr_,
                                numCols / kc_,
                                p1Prime,
                                p2Prime,
                                (j == q1 / (numCols / kc_)) * (q1 % (numCols / kc_)),
                                (j == q2 / (numCols / kc_)) ? q2 % (numCols / kc_) : numCols / kc_ - 1,
                                dp + (numRows / kr_) * i,
                                dq + (numCols / kc_) * j,
                                y + kc_ * i + j
                        );
                    }

                }

            }

        }

    }

    /* containsElement() */

    bool elemInRangeInit(size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            // dividing by k_ (as stated in the paper) in not correct,
            // because it does not use the size of the currently considered submatrix but of its submatrices
            if ((p1 == 0) && (q1 == 0) && (p2 == (numRows_ /*/ kr_*/ - 1)) && (q2 == (numCols_ /*/ kc_*/ - 1))) {
                return 1;
            }

            size_type p1Prime, p2Prime;

            for (auto i = p1 / (numRows_ / kr_); i <= p2 / (numRows_ / kr_); i++) {

                p1Prime = (i == p1 / (numRows_ / kr_)) * (p1 % (numRows_ / kr_));
                p2Prime = (i == p2 / (numRows_ / kr_)) ? (p2 % (numRows_ / kr_)) : numRows_ / kr_ - 1;

                for (auto j = q1 / (numCols_ / kc_); j <= q2 / (numCols_ / kc_); j++) {

                    if (elemInRange(numRows_ / kr_, numCols_ / kc_, p1Prime, p2Prime, (j == q1 / (numCols_ / kc_)) * (q1 % (numCols_ / kc_)), (j == q2 / (numCols_ / kc_)) ? q2 % (numCols_ / kc_) : numCols_ / kc_ - 1, kc_ * i + j)) {
                        return true;
                    }

                }

            }

        }

        return false;

    }

    bool elemInRange(size_type numRows, size_type numCols, size_type p1, size_type p2, size_type q1, size_type q2, size_type z) {

        if (z >= T_.size()) {

            return L_[z - T_.size()];

        } else {

            if (T_[z]) {

                // dividing by k_ (as stated in the paper) in not correct,
                // because it does not use the size of the currently considered submatrix but of its submatrices
                if ((p1 == 0) && (q1 == 0) && (p2 == (numRows /*/ kr_*/ - 1)) && (q2 == (numCols /*/ kc_*/ - 1))) {
                    return true;
                }

                auto y = R_.rank(z + 1) * kr_ * kc_;
                size_type p1Prime, p2Prime;

                for (auto i = p1 / (numRows / kr_); i <= p2 / (numRows / kr_); i++) {

                    p1Prime = (i == p1 / (numRows / kr_)) * (p1 % (numRows / kr_));
                    p2Prime = (i == p2 / (numRows / kr_)) ? (p2 % (numRows / kr_)) : numRows / kr_ - 1;

                    for (auto j = q1 / (numCols / kc_); j <= q2 / (numCols / kc_); j++) {

                        if (elemInRange(numRows / kr_, numCols / kc_, p1Prime, p2Prime, (j == q1 / (numCols / kc_)) * (q1 % (numCols / kc_)), (j == q2 / (numCols / kc_)) ? q2 % (numCols / kc_) : numCols / kc_ - 1, y + kc_ * i + j)) {
                            return true;
                        }

                    }

                }

            }

            return false;

        }

    }


    /* setNull() */

    void setInit(size_type p, size_type q) {

        if (!L_.empty()) {
            set(numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), q % (numCols_ / kc_), (p / (numRows_ / kr_)) * kc_ + q / (numCols_ / kc_));
        }

    }

    void set(size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {
            L_[z - T_.size()] = null_;
        } else {

            if (T_[z]) {
                set(numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_), R_.rank(z + 1) * kr_ * kc_ + (p / (numRows / kr_)) * kc_ + q / (numCols / kc_));
            }

        }

    }

};


/**
 * Bool specialisation of KrKcTree.
 *
 * Has the same characteristics as the general implementation above,
 * but makes use of some simplifications since the only non-null value is 1 / true.
 */
template<>
class KrKcTree<bool> : public virtual K2Tree<bool> {

public:
    typedef bool elem_type;

    typedef K2Tree<elem_type>::matrix_type matrix_type;
    typedef K2Tree<elem_type>::list_type list_type;
    typedef K2Tree<elem_type>::positions_type positions_type;
    typedef K2Tree<elem_type>::pairs_type pairs_type;


    KrKcTree() {
        // nothing to do
    }

    KrKcTree(const KrKcTree& other) {

        h_ = other.h_;
        kr_ = other.kr_;
        kc_ = other.kc_;
        numRows_ = other.numRows_;
        numCols_ = other.numCols_;
        null_ = other.null_;

        T_ = other.T_;
        L_ = other.L_;
        R_ = rank_type(&T_);

    }

    KrKcTree& operator=(const KrKcTree& other) {

        // check for self-assignment
        if (&other == this) {
            return *this;
        }

        h_ = other.h_;
        kr_ = other.kr_;
        kc_ = other.kc_;
        numRows_ = other.numRows_;
        numCols_ = other.numCols_;
        null_ = other.null_;

        T_ = other.T_;
        L_ = other.L_;
        R_ = rank_type(&T_);

        return *this;

    }

    /**
     * Matrix-based constructor (based on section 3.3.1. of Brisaboa et al.)
     *
     * Assumes that all rows of mat are equally long.
     */
    KrKcTree(const matrix_type& mat, const size_type kr, const size_type kc) {

        null_ = false;

        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(mat.size(), kr_), logK(mat[0].size(), kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        std::vector<std::vector<bool>> levels(h_);
        buildFromMatrix(mat, levels, numRows_, numCols_, 1, 0, 0);

        size_type total = 0;
        for (auto l = 0; l < h_ - 1; l++) {
            total += levels[l].size();
        }
        T_ = bit_vector_type(total);

        bit_vector_type::iterator outIter = T_.begin();
        for (size_type l = 0; l < h_ - 1; l++) {

            outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
            levels[l].clear();
            levels[l].shrink_to_fit();

        }

        L_ = bit_vector_type(levels[h_ - 1].size());
        std::move(levels[h_ - 1].begin(), levels[h_ - 1].end(), L_.begin());
        levels[h_ - 1].clear();
        levels[h_ - 1].shrink_to_fit();

        R_ = rank_type(&T_);

    }

    /**
     * Matrix-based constructor similar to the one above, but only a part of the relation matrix is used:
     *  x = first row of the submatrix
     *  y = first column of the submatrix
     *  nr = number of rows of the submatrix
     *  nc = number of columns of the submatrix
     */
    KrKcTree(const matrix_type& mat, const size_type x, const size_type y, const size_type nr, const size_type nc, const size_type kr, const size_type kc) {

        null_ = false;

        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(nr, kr_), logK(nc, kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        checkParameters(nr, nc, kr, kc);

        std::vector<std::vector<bool>> levels(h_);
        buildFromMatrix(mat, levels, numRows_, numCols_, 1, x, y);

        size_type total = 0;
        for (auto l = 0; l < h_ - 1; l++) {
            total += levels[l].size();
        }
        T_ = bit_vector_type(total);

        bit_vector_type::iterator outIter = T_.begin();
        for (size_type l = 0; l < h_ - 1; l++) {

            outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
            levels[l].clear();
            levels[l].shrink_to_fit();

        }

        L_ = bit_vector_type(levels[h_ - 1].size());
        std::move(levels[h_ - 1].begin(), levels[h_ - 1].end(), L_.begin());
        levels[h_ - 1].clear();
        levels[h_ - 1].shrink_to_fit();

        R_ = rank_type(&T_);

    }

    /**
     * List-of-lists-based constructor (based on sections 3.3.2. - 3.3.4. of Brisaboa et al.)
     *
     * The actually used method depends on parameter mode.
     */
    KrKcTree(const RelationLists& lists, const size_type kr, const size_type kc, const int mode) {

        null_ = false;

        size_type maxCol = 0;
        for (auto& row : lists) {
            for (auto& elem : row) {
                maxCol = std::max(maxCol, elem);
            }
        }
        maxCol++; // for number of columns

        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(lists.size(), kr_), logK(maxCol, kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        switch (mode) {

            case 0: { // 3.3.2.

                std::vector<std::vector<bool>> levels(h_);
                std::vector<RelationList::const_iterator> cursors;
                for (auto iter = lists.begin(); iter != lists.end(); iter++) {
                    cursors.push_back(iter->begin());
                }

                buildFromLists(lists, cursors, levels, numRows_, numCols_, 1, 0, 0);

                size_type total = 0;
                for (auto l = 0; l < h_ - 1; l++) {
                    total += levels[l].size();
                }
                T_ = bit_vector_type(total);

                bit_vector_type::iterator outIter = T_.begin();
                for (size_type l = 0; l < h_ - 1; l++) {

                    outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
                    levels[l].clear();
                    levels[l].shrink_to_fit();

                }

                L_ = bit_vector_type(levels[h_ - 1].size());
                std::move(levels[h_ - 1].begin(), levels[h_ - 1].end(), L_.begin());
                levels[h_ - 1].clear();
                levels[h_ - 1].shrink_to_fit();

                R_ = rank_type(&T_);

                break;

            }

            case 1: { // 3.3.3.

                buildFromListsViaTree(lists);

                R_ = rank_type(&T_);

                break;

            }

            case 2: { // 3.3.4.

                buildFromListsDynamicBitmaps(lists);

                break;

            }

            default: {

                buildFromListsDynamicBitmaps(lists);
                break;

            }

        }

    }

    /**
     * List-of-lists-based constructor similar to the one above, but only a part of the relation matrix is used:
     *  x = first row of the submatrix
     *  y = first column of the submatrix
     *  nr = number of rows of the submatrix
     *  nc = number of columns of the submatrix
     */
    KrKcTree(const RelationLists& lists, const size_type x, const size_type y, const size_type nr, const size_type nc, const size_type kr, const size_type kc, const int mode) {

        null_ = false;

        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(nr, kr_), logK(nc, kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        checkParameters(nr, nc, kr, kc);

        switch (mode) {

            case 0: { // 3.3.2.

                std::vector<std::vector<bool>> levels(h_);
                std::vector<RelationList::const_iterator> cursors;
                for (auto iter = lists.begin(); iter != lists.end(); iter++) {

                    cursors.push_back(iter->begin());
                    auto& rowIter = cursors.back();
                    while ((rowIter != iter->end()) && (*rowIter < y)) {
                        rowIter++;
                    }

                }

                buildFromLists(lists, cursors, levels, numRows_, numCols_, 1, x, y);

                size_type total = 0;
                for (auto l = 0; l < h_ - 1; l++) {
                    total += levels[l].size();
                }
                T_ = bit_vector_type(total);

                bit_vector_type::iterator outIter = T_.begin();
                for (size_type l = 0; l < h_ - 1; l++) {

                    outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
                    levels[l].clear();
                    levels[l].shrink_to_fit();

                }

                L_ = bit_vector_type(levels[h_ - 1].size());
                std::move(levels[h_ - 1].begin(), levels[h_ - 1].end(), L_.begin());
                levels[h_ - 1].clear();
                levels[h_ - 1].shrink_to_fit();

                R_ = rank_type(&T_);

                break;

            }

            case 1: { // 3.3.3.

                buildFromListsViaTree(lists, x, y, nr, nc);

                R_ = rank_type(&T_);

                break;

            }

            case 2: { // 3.3.4.

                buildFromListsDynamicBitmaps(lists, x, y, nr, nc);

                break;

            }

            default: {

                buildFromListsDynamicBitmaps(lists, x, y, nr, nc);
                break;

            }

        }

    }

    /**
     * List-of-pairs-based constructor (based on section 3.3.5. of Brisaboa et al.)
     */
    KrKcTree(positions_type& pairs, const size_type kr, const size_type kc) {

        null_ = false;

        size_type maxRow = 0;
        size_type maxCol = 0;
        for (auto p : pairs) {
            maxRow = std::max(maxRow, p.first);
            maxCol = std::max(maxCol, p.second);
        }


        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(maxRow + 1, kr_), logK(maxCol + 1, kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        if (pairs.size() != 0) {
            buildFromListsInplace(pairs);
        }

        R_ = rank_type(&T_);

    }

    /**
     * List-of-pairs-based constructor similar to the one above, but only a part of the relation matrix is used:
     *  x = first row of the submatrix
     *  y = first column of the submatrix
     *  nr = number of rows of the submatrix
     *  nc = number of columns of the submatrix
     */
    KrKcTree(positions_type& pairs, const size_type x, const size_type y, const size_type nr, const size_type nc, const size_type l, const size_type r, const size_type kr, const size_type kc) {

        null_ = false;

        kr_ = kr;
        kc_ = kc;
        h_ = std::max({(size_type)1, logK(nr, kr_), logK(nc, kc_)});
        numRows_ = size_type(pow(kr_, h_));
        numCols_ = size_type(pow(kc_, h_));

        checkParameters(nr, nc, kr, kc);

        if (l != r) {
            buildFromListsInplace(pairs, x, y, nr, nc, l, r);
        }

        R_ = rank_type(&T_);

    }


    // returns the height of the K2Tree
    size_type getH() {
        return h_;
    }

    // returns the row arity of the K2Tree
    size_type getKr() {
        return kr_;
    }

    // returns the column arity of the K2Tree
    size_type getKc() {
        return kc_;
    }

    size_type getNumRows() override {
        return numRows_;
    }

    size_type getNumCols() override {
        return numCols_;
    }

    elem_type getNull() override {
        return null_;
    }


    bool areRelated(size_type i, size_type j) override {
        return checkLinkInit(i, j);
    }

    std::vector<size_type> getSuccessors(size_type i) override {

        std::vector<size_type> succs;
//        successorsInit(succs, i);
        allSuccessorPositionsIterative(succs, i);

        return succs;

    }

    std::vector<size_type> getPredecessors(size_type j) override {

        std::vector<size_type> preds;
        predecessorsInit(preds, j);

        return preds;

    }

    positions_type getRange(size_type i1, size_type i2, size_type j1, size_type j2) override {

        positions_type pairs;
        rangeInit(pairs, i1, i2, j1, j2);
//        rangeInit(pairs, i1, std::min(i2, numRows_ - 1), j1, std::min(j2, numCols_ - 1));

        return pairs;

    }

    bool containsLink(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return linkInRangeInit(i1, i2, j1, j2);
//        return linkInRangeInit(i1, std::min(i2, numRows_ - 1), j1, std::min(j2, numCols_ - 1));
    }

    size_type countLinks() override {

        size_type res = 0;
        for (auto i = 0; i < L_.size(); i++) {
            res += L_[i];
        }

        return res;

    }


    /*
     * General methods for completeness' sake (are redundant / useless for bool)
     */

    bool isNotNull(size_type i, size_type j) override {
        return areRelated(i, j);
    }

    elem_type getElement(size_type i, size_type j) override {
        return areRelated(i, j);
    }

    std::vector<elem_type> getSuccessorElements(size_type i) override {
        return std::vector<elem_type>(getSuccessors(i).size(), true);
    }

    std::vector<size_type> getSuccessorPositions(size_type i) override {
        return getSuccessors(i);
    }

    pairs_type getSuccessorValuedPositions(size_type i) override {

        auto pos = getSuccessors(i);

        pairs_type succs;
        for (auto j : pos) {
            succs.push_back(ValuedPosition<elem_type>(i, j, true));
        }

        return succs;

    }

    std::vector<elem_type> getPredecessorElements(size_type j) override {
        return std::vector<elem_type>(getPredecessors(j).size(), true);
    }

    std::vector<size_type> getPredecessorPositions(size_type j) override {
        return getPredecessors(j);
    }

    pairs_type getPredecessorValuedPositions(size_type j) override {

        auto pos = getPredecessors(j);

        pairs_type preds;
        for (auto i : pos) {
            preds.push_back(ValuedPosition<elem_type>(i, j, true));
        }

        return preds;

    }

    std::vector<elem_type> getElementsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return std::vector<elem_type>(getRange(i1, i2, j1, j2).size(), true);
    }

    positions_type getPositionsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return getRange(i1, i2, j1, j2);
    }

    pairs_type getValuedPositionsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {

        auto pos = getRange(i1, i2, j1, j2);

        pairs_type pairs;
        for (auto& p : pos) {
            pairs.push_back(ValuedPosition<elem_type>(p.first, p.second, true));
        }

        return pairs;

    }

    std::vector<elem_type> getAllElements() override {
        return std::vector<elem_type>(countLinks(), true);
    }

    positions_type getAllPositions() override {
        return getRange(0, numRows_ - 1, 0, numCols_ - 1);
    }

    pairs_type getAllValuedPositions() override {

        auto pos = getAllPositions();

        pairs_type pairs;
        for (auto& p : pos) {
            pairs.push_back(ValuedPosition<elem_type>(p.first, p.second, true));
        }

        return pairs;

    }

    bool containsElement(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return linkInRangeInit(i1, i2, j1, j2);
//        return linkInRangeInit(i1, std::min(i2, numRows_ - 1), j1, std::min(j2, numCols_ - 1));
    }

    size_type countElements() override {
        return countLinks();
    }


    KrKcTree* clone() const override {
        return new KrKcTree<elem_type>(*this);
    }

    void print(bool all = false) override {

        std::cout << "### Parameters ###" << std::endl;
        std::cout << "h  = " << h_ << std::endl;
        std::cout << "kr  = " << kr_ << std::endl;
        std::cout << "kc  = " << kc_ << std::endl;
        std::cout << "numRows = " << numRows_ << std::endl;
        std::cout << "numCols = " << numCols_ << std::endl;
        std::cout << "null = " << null_ << std::endl;

        if (all) {

            std::cout << "### T ###" << std::endl;
            for (auto i = 0; i < T_.size(); i++) std::cout << T_[i];
            std::cout << std::endl << std::endl;

            std::cout << "### L ###" << std::endl;
            for (auto i = 0; i < L_.size(); i++) std::cout << L_[i];
            std::cout << std::endl << std::endl;

            std::cout << "### R ###" << std::endl;
            printRanks(R_);
            std::cout << std::endl;

        }

    }

    // note: can "invalidate" the data structure (containsLink() probably won't work correctly afterwards)
    void setNull(size_type i, size_type j) override {
        setInit(i, j);
    }

    size_type getFirstSuccessor(size_type i) override {
//        return firstSuccessorInit(i);
        return firstSuccessorPositionIterative(i);
    }



private:
    // representation of all but the last levels of the K2Tree (internal structure)
    bit_vector_type T_;

    // representation of the last level of the K2Tree (actual values of the relation)
    bit_vector_type L_;

    // rank data structure for navigation in T_
    rank_type R_;

    size_type h_; // height of the K2Tree
    size_type kr_; // row arity of the K2Tree
    size_type kc_; // column arity of the K2Tree
    size_type numRows_; // number of rows in the represented relation matrix
    size_type numCols_; // number of columns in the represented relation matrix

    elem_type null_; // null element


    /* helper method to check the feasibility of the tree parameters prior to construction */

    void checkParameters(const size_type nr, const size_type nc, const size_type kr, const size_type kc) {

        if ((numRows_ != nr) || (numCols_ != nc)) {

            std::string err = std::string() +
                              "Unsuitable parameters! " +
                              "The numbers of rows (nr) and columns (nc) have to be powers of kr resp. kc (using the same exponent h)." +
                              "But you gave me: nr = " + std::to_string(nr) + ", nc = " = std::to_string(nc) +
                                                                                          ", kr = " + std::to_string(kr) + " and kc = " + std::to_string(kc) + " leading to h = " + std::to_string(h_) +
                                                                                          " and " + std::to_string(numRows_) + " rows resp. " + std::to_string(numCols_) + " columns."
            ;

            throw std::runtime_error(err);

        }

    }

    /* helper method for construction from relation matrix */

    bool buildFromMatrix(const RelationMatrix& mat, std::vector<std::vector<bool>>& levels, size_type numRows, size_type numCols, size_type l, size_type p, size_type q) {// 3.3.1 / Algorithm 1

        std::vector<bool> C;

        if (l == h_) {

            for (auto i = 0; i < kr_; i++) {
                for (auto j = 0; j < kc_; j++) {
                    C.push_back((((p + i) < mat.size()) && ((q + j) < mat[0].size())) ? mat[p + i][q + j] : false);
                }
            }

        } else {

            for (auto i = 0; i < kr_; i++) {
                for (auto j = 0; j < kc_; j++) {
                    C.push_back(buildFromMatrix(mat, levels, numRows / kr_, numCols / kc_, l + 1, p + i * (numRows / kr_), q + j * (numCols / kc_)));
                }
            }

        }

        if (isAllZero(C)) {
            return false;
        } else {

            levels[l - 1].insert(levels[l - 1].end(), C.begin(), C.end());
            return true;

        }

    }

    /* helper method for construction from relation lists */

    bool buildFromLists(const RelationLists& lists, std::vector<RelationList::const_iterator>& cursors, std::vector<std::vector<bool>>& levels, size_type numRows, size_type numCols, size_type l, size_type p, size_type q) {// 3.3.2

        std::vector<bool> C;

        if (l == h_) {

            for (auto i = 0; i < kr_; i++) {
                for (auto j = 0; j < kc_; j++) {

                    C.push_back(((p + i) < lists.size()) && (cursors[p + i] != lists[p + i].end()) && ((q + j) == *(cursors[p + i])));
                    if (C.back()) cursors[p + i]++;

                }
            }

        } else {

            for (auto i = 0; i < kr_; i++) {
                for (auto j = 0; j < kc_; j++) {
                    C.push_back(buildFromLists(lists, cursors, levels, numRows / kr_, numCols / kc_, l + 1, p + i * (numRows / kr_), q + j * (numCols / kc_)));
                }
            }

        }

        if (isAllZero(C)) {
            return false;
        } else {

            levels[l - 1].insert(levels[l - 1].end(), C.begin(), C.end());
            return true;

        }

    }

    /* helper methods for construction from relation lists via temporary tree */

    void buildFromListsViaTree(const RelationLists& lists) {// 3.3.3, so far without special bit vectors without initialisation

        Node<bool>* root = new Node<bool>(false);

        for (size_type i = 0; i < lists.size(); i++) {
            for (size_type j = 0; j < lists[i].size(); j++) {
                insert(root, numRows_, numCols_, i, lists[i][j]);
            }
        }

        if (!root->isLeaf()) {

            std::vector<bool> T, L;

            // traverse over tree and generate T and L while doing it
            std::queue<Node<bool>*> queue;
            Node<bool>* node;
            Node<bool>* child;
            queue.push(root);

            while (!queue.empty()) {

                node = queue.front();
                queue.pop();

                for (size_type i = 0; i < kr_ * kc_; i++) {

                    child = node->getChild(i);

                    if (child != 0 && child->isLeaf()) {
                        L.push_back(child->getLabel());
                    } else {

                        T.push_back(child != 0);

                        if (T.back()) {
                            queue.push(child);
                        }

                    }

                }

            }

            L_ = bit_vector_type(L.size());
            std::move(L.begin(), L.end(), L_.begin());
            L.clear();
            L.shrink_to_fit();

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        delete root;

    }

    void buildFromListsViaTree(const RelationLists& lists, size_type x, size_type y, size_type nr, size_type nc) {// 3.3.3, so far without special bit vectors without initialisation

        Node<bool>* root = new Node<bool>(false);

        for (size_type i = x; (i < x + nr) && (i < lists.size()); i++) {
            for (size_type j = 0; j < lists[i].size(); j++) {
                if ((y <= lists[i][j]) && (lists[i][j] < (y + nc))) {
                    insert(root, nr, nc, i - x, lists[i][j] - y);
                }
            }
        }

        if (!root->isLeaf()) {

            std::vector<bool> T, L;

            // traverse over tree and generate T and L while doing it
            std::queue<Node<bool>*> queue;
            Node<bool>* node;
            Node<bool>* child;
            queue.push(root);

            while (!queue.empty()) {

                node = queue.front();
                queue.pop();

                for (size_type i = 0; i < kr_ * kc_; i++) {

                    child = node->getChild(i);

                    if (child != 0 && child->isLeaf()) {
                        L.push_back(child->getLabel());
                    } else {

                        T.push_back(child != 0);

                        if (T.back()) {
                            queue.push(child);
                        }

                    }

                }

            }

            L_ = bit_vector_type(L.size());
            std::move(L.begin(), L.end(), L_.begin());
            L.clear();
            L.shrink_to_fit();

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        delete root;

    }

    void insert(Node<bool>* node, size_type numRows, size_type numCols, size_type p, size_type q) {

        if (numRows == kr_) { // alternatively, numCols == kc_; occurs (currently) at the same time since we use the same height in both dimensions

            if (node->isLeaf()) {
                node->turnInternal(kr_ * kc_, true);
            }

            node->addChild(p * kc_ + q, true);

        } else {

            if (node->isLeaf()) {
                node->turnInternal(kr_ * kc_, false);
            }

            size_type z = (p / (numRows / kr_)) * kc_ + q / (numCols / kc_);

            insert(node->hasChild(z) ? node->getChild(z) : node->addChild(z, true), numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_));

        }

    }

    /* helper methods for construction from relation lists via dynamic bitmap representations */

    void buildFromListsDynamicBitmaps(const RelationLists& lists) {// 3.3.4, currently no succinct dynamic bitmaps

        if (h_ == 1) {

            L_ = bit_vector_type(kr_ * kc_, 0);

            for (size_type i = 0; i < lists.size(); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    L_[i * kc_ + lists[i][j]] = 1;
                }
            }

            if (isAllZero(L_)) {
                L_ = bit_vector_type(0);
            }

        } else {

            std::vector<bool> T, L;
            NaiveDynamicRank R;

            for (size_type i = 0; i < lists.size(); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    insertInit(T, L, R, i, lists[i][j]);
                }
            }

            L_ = bit_vector_type(L.size());
            std::move(L.begin(), L.end(), L_.begin());
            L.clear();
            L.shrink_to_fit();

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        R_ = rank_type(&T_);

    }

    void buildFromListsDynamicBitmaps(const RelationLists& lists, size_type x, size_type y, size_type nr, size_type nc) {// 3.3.4, currently no succinct dynamic bitmaps

        if (h_ == 1) {

            L_ = bit_vector_type(kr_ * kc_, 0);

            for (size_type i = x; (i < x + nr) && (i < lists.size()); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    if ((y <= lists[i][j]) && (lists[i][j] < (y + nc))) {
                        L_[(i - x) * kc_ + lists[i][j] - y] = 1;
                    }
                }
            }

            if (isAllZero(L_)) {
                L_ = bit_vector_type(0);
            }

        } else {

            std::vector<bool> T, L;
            NaiveDynamicRank R;

            for (size_type i = x; (i < x + nr) && (i < lists.size()); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    if ((y <= lists[i][j]) && (lists[i][j] < (y + nc))) {
                        insertInit(T, L, R, i - x, lists[i][j] - y);
                    }
                }
            }

            L_ = bit_vector_type(L.size());
            std::move(L.begin(), L.end(), L_.begin());
            L.clear();
            L.shrink_to_fit();

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        R_ = rank_type(&T_);

    }

    void insertInit(std::vector<bool>& T, std::vector<bool>& L, NaiveDynamicRank& R, size_type p, size_type q) {

        if (T.empty()) {

            T = std::vector<bool>(kr_ * kc_);
            R = NaiveDynamicRank(T);

        }

        insert(T, L, R, numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), q % (numCols_ / kc_), (p / (numRows_ / kr_)) * kc_ + q / (numCols_ / kc_), 1);

    }

    void insert(std::vector<bool>& T, std::vector<bool>& L, NaiveDynamicRank& R, size_type numRows, size_type numCols, size_type p, size_type q, size_type z, size_type l) {

        if (!T[z]) {

            T[z] = 1;
            R.increaseFrom(z + 1);

            size_type y = R.rank(z + 1) * kr_ * kc_ + (p / (numRows / kr_)) * kc_ + q / (numCols / kc_);

            if ((l + 1) == h_) {

                L.insert(L.begin() + R.rank(z + 1) * kr_ * kc_ - T.size(), kr_ * kc_, 0);
                L[y - T.size()] = 1;

            } else {

                T.insert(T.begin() + R.rank(z + 1) * kr_ * kc_, kr_ * kc_, 0);
                R.insert(R.rank(z + 1) * kr_ * kc_ + 1, kr_ * kc_);

                insert(T, L, R, numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_), y, l + 1);

            }

        } else {

            size_type y = R.rank(z + 1) * kr_ * kc_ + (p / (numRows / kr_)) * kc_ + q / (numCols / kc_);

            if ((l + 1) == h_) {
                L[y - T.size()] = 1;
            } else {
                insert(T, L, R, numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_), y, l + 1);
            }

        }

    }

    /* helper methods for inplace construction from single list of pairs */

    size_type computeKey(const positions_type::value_type& pair, const Subproblem& sp, size_type widthRow, size_type widthCol) {
        return ((pair.first - sp.firstRow) / widthRow) * kc_ + (pair.second - sp.firstCol) / widthCol;
    }

    void countingSort(positions_type& pairs, std::vector<std::pair<size_type, size_type>>& intervals, const Subproblem& sp, size_type widthRow, size_type widthCol, size_type sup) {

        std::vector<size_type> counts(sup);

        // determine key frequencies
        for (auto i = sp.left; i < sp.right; i++) {
            counts[computeKey(pairs[i], sp, widthRow, widthCol)]++;
        }

        // determine starting index for each key
        size_type total = 0;
        size_type tmp;

        for (auto key = 0; key < sup; key++) {

            tmp = counts[key];
            counts[key] = total;
            total += tmp;

            intervals[key].first = counts[key];
            intervals[key].second = total;

        }

        // reorder pairs of current subproblem
        positions_type tmpPairs(sp.right - sp.left + 1);
        for (auto i = sp.left; i < sp.right; i++) {

            tmpPairs[counts[computeKey(pairs[i], sp, widthRow, widthCol)]] = pairs[i];
            counts[computeKey(pairs[i], sp, widthRow, widthCol)]++;

        }

        for (auto i = sp.left; i < sp.right; i++) {
            pairs[i] = tmpPairs[i - sp.left];
        }

    }

    void buildFromListsInplace(positions_type& pairs) {// 3.3.5

        std::queue<Subproblem> queue;
        Subproblem sp;
        size_type Sr, Sc;
        std::vector<std::pair<size_type, size_type>> intervals(kr_ * kc_);
        std::vector<bool> T, L;
        bit_vector_type appToL;

        queue.push(Subproblem(0, numRows_ - 1, 0, numCols_ - 1, 0, pairs.size()));

        while (!queue.empty()) {

            sp = queue.front();
            queue.pop();

            Sr = sp.lastRow - sp.firstRow + 1;
            Sc = sp.lastCol - sp.firstCol + 1;

            if (Sr > kr_) {

                countingSort(pairs, intervals, sp, Sr / kr_, Sc / kc_, kr_ * kc_);

                for (size_type i = 0; i < kr_ * kc_; i++) {

                    if (intervals[i].first < intervals[i].second) {

                        T.push_back(true);
                        queue.push(Subproblem(
                                sp.firstRow + (i / kc_) * (Sr / kr_),
                                sp.firstRow + (i / kc_ + 1) * (Sr / kr_) - 1,
                                sp.firstCol + (i % kc_) * (Sc / kc_),
                                sp.firstCol + (i % kc_ + 1) * (Sc / kc_) - 1,
                                sp.left + intervals[i].first,
                                sp.left + intervals[i].second
                        ));

                    } else {
                        T.push_back(false);
                    }

                }

            } else {

                appToL = bit_vector_type(kr_ * kc_);

                for (size_type i = sp.left; i < sp.right; i++) {
                    appToL[(pairs[i].first - sp.firstRow) * kc_ + (pairs[i].second - sp.firstCol)] = true;
                }

                L.insert(L.end(), appToL.begin(), appToL.end());

            }

        }

        L_ = bit_vector_type(L.size());
        std::move(L.begin(), L.end(), L_.begin());
        L.clear();
        L.shrink_to_fit();

        T_ = bit_vector_type(T.size());
        std::move(T.begin(), T.end(), T_.begin());

    }

    void buildFromListsInplace(positions_type& pairs, size_type x, size_type y, size_type nr, size_type nc, size_type l, size_type r) {// 3.3.5

        std::queue<Subproblem> queue;
        Subproblem sp;
        size_type Sr, Sc;
        std::vector<std::pair<size_type, size_type>> intervals(kr_ * kc_);
        std::vector<bool> T, L;
        bit_vector_type appToL;

        queue.push(Subproblem(x, x + nr - 1, y, y + nc - 1, l, r));

        while (!queue.empty()) {

            sp = queue.front();
            queue.pop();

            Sr = sp.lastRow - sp.firstRow + 1;
            Sc = sp.lastCol - sp.firstCol + 1;

            if (Sr > kr_) {

                countingSort(pairs, intervals, sp, Sr / kr_, Sc / kc_, kr_ * kc_);

                for (size_type i = 0; i < kr_ * kc_; i++) {

                    if (intervals[i].first < intervals[i].second) {

                        T.push_back(true);
                        queue.push(Subproblem(
                                sp.firstRow + (i / kc_) * (Sr / kr_),
                                sp.firstRow + (i / kc_ + 1) * (Sr / kr_) - 1,
                                sp.firstCol + (i % kc_) * (Sc / kc_),
                                sp.firstCol + (i % kc_ + 1) * (Sc / kc_) - 1,
                                sp.left + intervals[i].first,
                                sp.left + intervals[i].second
                        ));

                    } else {
                        T.push_back(false);
                    }

                }

            } else {

                appToL = bit_vector_type(kr_ * kc_);

                for (size_type i = sp.left; i < sp.right; i++) {
                    appToL[(pairs[i].first - sp.firstRow) * kc_ + (pairs[i].second - sp.firstCol)] = true;
                }

                L.insert(L.end(), appToL.begin(), appToL.end());

            }

        }

        L_ = bit_vector_type(L.size());
        std::move(L.begin(), L.end(), L_.begin());
        L.clear();
        L.shrink_to_fit();

        T_ = bit_vector_type(T.size());
        std::move(T.begin(), T.end(), T_.begin());

    }


    /* areRelated() */

    bool checkLinkInit(size_type p, size_type q) {
        return (L_.empty()) ? false : checkLink(numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), q % (numCols_ / kc_), (p / (numRows_ / kr_)) * kc_ + q / (numCols_ / kc_));
    }

    bool checkLink(size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {
            return L_[z - T_.size()];
        } else {
            return T_[z] ? checkLink(numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_), R_.rank(z + 1) * kr_ * kc_ + (p / (numRows / kr_)) * kc_ + q / (numCols / kc_)) : false;
        }

    }

    /* getSuccessors() */

    void allSuccessorPositionsIterative(std::vector<size_type>& succs, size_type p) {

        if (L_.empty()) return;

        std::queue<SubrowInfo> queue, nextLevelQueue;
        size_type lenT = T_.size();

        if (lenT == 0) {

            size_type offset = p * numCols_;
            for (size_type i = 0; i < numCols_; i++) {
                if (L_[offset + i]) {
                    succs.push_back(i);
                }
            }

        } else {

            // successorsPosInit
            size_type nr = numRows_/ kr_;
            size_type nc = numCols_/ kc_;
            size_type relP = p;
            for (size_type j = 0, dq = 0, z = kc_ * (relP / nr); j < kc_; j++, dq += nc, z++) {
                queue.push(SubrowInfo(dq, z));
            }

            // successorsPos
            relP %= nr;
            nr /= kr_;
            nc /= kc_;
            for (; nr > 1; relP %= nr, nr /= kr_, nc /= kc_) {

                while (!queue.empty()) {

                    auto& cur = queue.front();

                    if (T_[cur.z]) {

                        auto y = R_.rank(cur.z + 1) * kr_ * kc_ + kc_ * (relP / nr);

                        for (size_type j = 0, newDq = cur.dq; j < kc_; j++, newDq += nc, y++) {
                            nextLevelQueue.push(SubrowInfo(newDq, y));
                        }

                    }

                    queue.pop();

                }

                queue.swap(nextLevelQueue);

            }


            while (!queue.empty()) {

                auto& cur = queue.front();

                if (T_[cur.z]) {

                    auto y = R_.rank(cur.z + 1) * kr_ * kc_ + kc_ * (relP / nr) - lenT;

                    for (size_type j = 0, newDq = cur.dq; j < kc_; j++, newDq += nc, y++) {
                        if (L_[y]) {
                            succs.push_back(newDq);
                        }
                    }

                }

                queue.pop();

            }

        }

    }

    void successorsInit(std::vector<size_type>& succs, size_type p) {

        if (!L_.empty()) {

            size_type y = kc_ * (p / (numRows_ / kr_));

            for (size_type j = 0; j < kc_; j++) {
                successors(succs, numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), (numCols_ / kc_) * j, y + j);
            }

        }

    }

    void successors(std::vector<size_type>& succs, size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()]) {
                succs.push_back(q);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * kr_ * kc_ + kc_ * (p / (numRows / kr_));

                for (size_type j = 0; j < kc_; j++) {
                    successors(succs, numRows / kr_, numCols / kc_, p % (numRows / kr_), q + (numCols / kc_) * j, y + j);
                }

            }

        }


    }

    /* getFirstSuccessor() */

    size_type firstSuccessorPositionIterative(size_type p) {

        if (L_.empty()) return numCols_;

        if (T_.size() == 0) {

            size_type offset = p * numCols_;
            for (size_type i = 0; i < numCols_; i++) {
                if (L_[offset + i]) {
                    return i;
                }
            }

        } else {

            std::stack<ExtendedSubrowInfo> stack;
            stack.emplace(numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), 0, kc_ * (p / (numRows_ / kr_)), 0);

            while (!stack.empty()) {

                auto& cur = stack.top();

                if (cur.j == kc_) {
                    stack.pop();
                } else {

                    if (cur.z >= T_.size()) {

                        if (L_[cur.z - T_.size()]) {
                            return cur.dq;
                        }

                    } else {

                        if (T_[cur.z]) {
                            stack.emplace(cur.nr / kr_, cur.nc / kc_, cur.p % (cur.nr / kr_), cur.dq, R_.rank(cur.z + 1) * kr_ * kc_ + kc_ * (cur.p / (cur.nr / kr_)), 0);
                        }

                    }

                    cur.dq += cur.nc;
                    cur.z++;
                    cur.j++;

                }

            }
        }

        return numCols_;

    }

    size_type firstSuccessorInit(size_type p) {

        size_type pos = numCols_;

        if (!L_.empty()) {

            size_type y = kc_ * (p / (numRows_ / kr_));

            for (size_type j = 0; j < kc_ && pos == numCols_; j++) {
                pos = firstSuccessor(numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), (numCols_ / kc_) * j, y + j);
            }

        }

        return pos;

    }

    size_type firstSuccessor(size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        size_type pos = numCols_;

        if (z >= T_.size()) {

            if (L_[z - T_.size()]) {
                pos = q;
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * kr_ * kc_ + kc_ * (p / (numRows / kr_));

                for (size_type j = 0; j < kc_ && pos == numCols_; j++) {
                    pos = firstSuccessor(numRows / kr_, numCols / kc_, p % (numRows / kr_), q + (numCols / kc_) * j, y + j);
                }

            }

        }

        return pos;

    }

    /* getPredecessors() */

    void predecessorsInit(std::vector<size_type>& preds, size_type q) {

        if (!L_.empty()) {

            size_type y = q / (numCols_ / kc_);

            for (size_type i = 0; i < kr_; i++) {
                predecessors(preds, numRows_ / kr_, numCols_ / kc_, q % (numCols_ / kc_), (numRows_ / kr_) * i, y + i * kc_);
            }

        }

    }

    void predecessors(std::vector<size_type>& preds, size_type numRows, size_type numCols, size_type q, size_type p, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()]) {
                preds.push_back(p);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * kr_ * kc_ + q / (numCols / kc_);

                for (size_type i = 0; i < kr_; i++) {
                    predecessors(preds, numRows / kr_, numCols / kc_,  q % (numCols / kc_), p + (numRows / kr_) * i, y + i * kc_);
                }

            }

        }

    }

    /* getRange() */

    void rangeInit(positions_type& pairs, size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            size_type p1Prime, p2Prime;

            for (auto i = p1 / (numRows_ / kr_); i <= p2 / (numRows_ / kr_); i++) {

                p1Prime = (i == p1 / (numRows_ / kr_)) * (p1 % (numRows_ / kr_));
                p2Prime = (i == p2 / (numRows_ / kr_)) ? p2 % (numRows_ / kr_) : (numRows_ / kr_) - 1;

                for (auto j = q1 / (numCols_ / kc_); j <= q2 / (numCols_ / kc_); j++) {
                    range(
                            pairs,
                            numRows_ / kr_,
                            numCols_ / kc_,
                            p1Prime,
                            p2Prime,
                            (j == q1 / (numCols_ / kc_)) * (q1 % (numCols_ / kc_)),
                            (j == q2 / (numCols_ / kc_)) ? q2 % (numCols_ / kc_) : (numCols_ / kc_) - 1,
                            (numRows_ / kr_) * i,
                            (numCols_ / kc_) * j,
                            kc_ * i + j
                    );
                }

            }

        }

    }

    void range(positions_type& pairs, size_type numRows, size_type numCols, size_type p1, size_type p2, size_type q1, size_type q2, size_type dp, size_type dq, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()]) {
                pairs.push_back(std::make_pair(dp, dq));
            }

        } else {

            if (T_[z]) {

                auto y = R_.rank(z + 1) * kr_ * kc_;
                size_type p1Prime, p2Prime;

                for (auto i = p1 / (numRows / kr_); i <= p2 / (numRows / kr_); i++) {

                    p1Prime = (i == p1 / (numRows / kr_)) * (p1 % (numRows / kr_));
                    p2Prime = (i == p2 / (numRows / kr_)) ? p2 % (numRows / kr_) : numRows / kr_ - 1;

                    for (auto j = q1 / (numCols / kc_); j <= q2 / (numCols / kc_); j++) {
                        range(
                                pairs,
                                numRows / kr_,
                                numCols / kc_,
                                p1Prime,
                                p2Prime,
                                (j == q1 / (numCols / kc_)) * (q1 % (numCols / kc_)),
                                (j == q2 / (numCols / kc_)) ? q2 % (numCols / kc_) : numCols / kc_ - 1,
                                dp + (numRows / kr_) * i,
                                dq + (numCols / kc_) * j,
                                y + kc_ * i + j
                        );
                    }

                }

            }

        }

    }

    /* linkInRange() */

    bool linkInRangeInit(size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            // dividing by k_ (as stated in the paper) in not correct,
            // because it does not use the size of the currently considered submatrix but of its submatrices
            if ((p1 == 0) && (q1 == 0) && (p2 == (numRows_ /*/ kr_*/ - 1)) && (q2 == (numCols_ /*/ kc_*/ - 1))) {
                return 1;
            }

            size_type p1Prime, p2Prime;

            for (auto i = p1 / (numRows_ / kr_); i <= p2 / (numRows_ / kr_); i++) {

                p1Prime = (i == p1 / (numRows_ / kr_)) * (p1 % (numRows_ / kr_));
                p2Prime = (i == p2 / (numRows_ / kr_)) ? (p2 % (numRows_ / kr_)) : numRows_ / kr_ - 1;

                for (auto j = q1 / (numCols_ / kc_); j <= q2 / (numCols_ / kc_); j++) {

                    if (linkInRange(numRows_ / kr_, numCols_ / kc_, p1Prime, p2Prime, (j == q1 / (numCols_ / kc_)) * (q1 % (numCols_ / kc_)), (j == q2 / (numCols_ / kc_)) ? q2 % (numCols_ / kc_) : numCols_ / kc_ - 1, kc_ * i + j)) {
                        return true;
                    }

                }

            }

        }

        return false;

    }

    bool linkInRange(size_type numRows, size_type numCols, size_type p1, size_type p2, size_type q1, size_type q2, size_type z) {

        if (z >= T_.size()) {

            return L_[z - T_.size()];

        } else {

            if (T_[z]) {

                // dividing by k_ (as stated in the paper) in not correct,
                // because it does not use the size of the currently considered submatrix but of its submatrices
                if ((p1 == 0) && (q1 == 0) && (p2 == (numRows /*/ kr_*/ - 1)) && (q2 == (numCols /*/ kc_*/ - 1))) {
                    return true;
                }

                auto y = R_.rank(z + 1) * kr_ * kc_;
                size_type p1Prime, p2Prime;

                for (auto i = p1 / (numRows / kr_); i <= p2 / (numRows / kr_); i++) {

                    p1Prime = (i == p1 / (numRows / kr_)) * (p1 % (numRows / kr_));
                    p2Prime = (i == p2 / (numRows / kr_)) ? (p2 % (numRows / kr_)) : numRows / kr_ - 1;

                    for (auto j = q1 / (numCols / kc_); j <= q2 / (numCols / kc_); j++) {

                        if (linkInRange(numRows / kr_, numCols / kc_, p1Prime, p2Prime, (j == q1 / (numCols / kc_)) * (q1 % (numCols / kc_)), (j == q2 / (numCols / kc_)) ? q2 % (numCols / kc_) : numCols / kc_ - 1, y + kc_ * i + j)) {
                            return true;
                        }

                    }

                }

            }

            return false;

        }

    }


    /* setNull() */

    void setInit(size_type p, size_type q) {

        if (!L_.empty()) {
            set(numRows_ / kr_, numCols_ / kc_, p % (numRows_ / kr_), q % (numCols_ / kc_), (p / (numRows_ / kr_)) * kc_ + q / (numCols_ / kc_));
        }

    }

    void set(size_type numRows, size_type numCols, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {
            L_[z - T_.size()] = null_;
        } else {

            if (T_[z]) {
                set(numRows / kr_, numCols / kc_, p % (numRows / kr_), q % (numCols / kc_), R_.rank(z + 1) * kr_ * kc_ + (p / (numRows / kr_)) * kc_ + q / (numCols / kc_));
            }

        }

    }

};

#endif //K2TREES_STATICBASICRECTANGULARTREE_HPP
