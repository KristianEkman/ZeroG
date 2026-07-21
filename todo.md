**Some positions carry much more useful learning information than others.** Positions where a deeper search discovers something missed by the static evaluation can be especially valuable—but only when the discovered advantage is something the evaluator can reasonably learn.

## The important distinction

Suppose you record:

[
s_{\text{static}} = \text{NNUE or handcrafted evaluation}
]

[
s_{\text{deep}} = \text{score after a deeper search}
]

and define:

[
\Delta = |s_{\text{deep}}-s_{\text{static}}|
]

A large (\Delta) means the position exposes a weakness in the evaluator. This is essentially **hard-example mining**.

But large-(\Delta) positions fall into different categories.

### 1. Learnable positional mistakes

Example:

* Static evaluation: (+0.10)
* Depth 8: (+0.30)
* Depth 14: (+1.10)
* Depth 16: (+1.05)

Perhaps the deeper search recognizes:

* a permanently weak king;
* a trapped piece;
* a strong passed pawn;
* a bad bishop;
* a favorable endgame;
* a long-term space advantage.

These positions are excellent training examples. The deeper value is stable, and the board features contain evidence for the advantage.

### 2. Pure tactical horizon effects

Example:

* Static evaluation: (0.00)
* Depth 8: (+0.10)
* Depth 14: (+5.00)

But the reason is a forced seven-move tactical sequence.

This position may not be particularly useful for training a **static** evaluator. Your network might not have enough information or capacity to calculate that sequence. Trying to make it output (+5) directly could distort its evaluation of superficially similar positions.

Usually, the better training position is the **quiet position reached at the end of the tactical sequence**, where the advantage is visible statically.

This is why quiet or tactically stable positions are commonly emphasized for NNUE datasets. Quiescence search has the same purpose: avoid evaluating a position in the middle of an unresolved tactical sequence. A recent dataset study also found that filtering unstable positions improved NNUE training, although its experiments were primarily in Xiangqi rather than Western chess. ([arXiv][1])

### 3. Unstable or noisy search results

Example:

* Depth 10: (+0.2)
* Depth 12: (+1.7)
* Depth 14: (-0.8)
* Depth 16: (+2.1)

This is not yet a reliable label. It might be:

* a highly tactical position;
* search instability;
* aspiration-window effects;
* a transposition-table issue;
* insufficient search depth;
* multiple roughly equal moves with very different evaluations.

Do not give such a position extra weight merely because its shallow and deep scores differ.

## A better criterion: surprising **and stable**

For each candidate position, calculate something like:

[
\Delta_{\text{hard}}
====================

|s_{\text{deep}}-s_{\text{qsearch}}|
]

and:

[
\Delta_{\text{stable}}
======================

|s_{\text{deep+extra}}-s_{\text{deep}}|
]

Then prefer positions where:

[
\Delta_{\text{hard}} \text{ is large}
]

but:

[
\Delta_{\text{stable}} \text{ is small}.
]

For example:

```text
Interesting:
    abs(deep_score - qsearch_score) > 100 cp
    abs(deeper_score - deep_score)  < 30 cp

Unreliable:
    abs(deeper_score - deep_score) > 100 cp
```

The exact thresholds depend on your engine. Values such as 80–150 cp for “interesting” and 25–50 cp for “stable” are reasonable initial experiments, not universal rules.

## Do not train only on interesting positions

If you train predominantly on large-error positions, you change the training distribution. The network may become good at unusual tactical or strategically difficult positions while becoming worse at normal positions.

A reasonable initial dataset mixture could be:

* **60–70% ordinary, representative positions**
* **15–25% hard but stable positions**
* **10–15% rare or underrepresented positions**

The normal positions teach calibration. The hard positions teach corrections. The rare positions prevent blind spots.

## Other things that make positions valuable

### Diversity

Do not save every position from every game. Consecutive positions are extremely correlated.

A good starting point is:

* save one position every 4–8 plies;
* limit the number of positions from one game;
* deduplicate identical positions;
* optionally deduplicate positions with the same pawn structure and very similar material;
* split training and validation data by **game**, not randomly by position.

You should balance:

* opening, middlegame and endgame;
* closed and open positions;
* kingside and queenside castling;
* opposite-side castling;
* different pawn structures;
* material imbalances;
* winning, losing and drawn evaluations;
* positions from both sides to move.

Dataset diversity is important because missing position classes create evaluation blind spots. ([arXiv][1])

### Evaluation range

A million positions evaluated at (+8) are usually less informative than positions around equality.

Positions near:

[
-2 \lesssim s \lesssim +2
]

often matter most because small evaluation errors can affect move selection and game outcome. But you still need clearly winning and losing examples to teach conversion and defensive evaluation.

Use evaluation buckets, for example:

```text
0–25 cp
25–75 cp
75–150 cp
150–300 cp
300–600 cp
600+ cp
```

Then cap or rebalance oversized buckets.

### Model disagreement

Once you already have a trained network, compare:

[
s_{\text{NN}} \quad\text{against}\quad s_{\text{deep}}.
]

Positions with large:

[
|s_{\text{NN}}-s_{\text{deep}}|
]

are especially interesting for the next training iteration. This is more direct than merely comparing shallow-search and deep-search values.

However, apply the stability and quietness filters first.

## Labels for the NNUE

A useful label usually combines:

1. a sufficiently strong search evaluation;
2. the final game result.

Convert the centipawn score to a win/draw/loss-like value:

[
p_{\text{eval}}
===============

\sigma\left(\frac{s}{K}\right),
]

where (K) is fitted to your engine’s score scale. Then use:

[
y
=

\lambda p_{\text{eval}}
+
(1-\lambda)z,
]

where:

* (z=1) for a win;
* (z=0.5) for a draw;
* (z=0) for a loss.

The Stockfish NNUE documentation describes this interpolation between search evaluation and game result. It also notes that converting centipawn scores to WDL space prevents huge evaluations from dominating the gradients. ([official-stockfish.github.io][2])

For an initial experiment, you might test:

```text
lambda = 0.7, 0.8, 0.9
```

A high (\lambda) follows your search teacher closely. A lower (\lambda) makes the final game outcome more influential.

## Handcrafted parameter tuning is slightly different

For Texel-style parameter tuning, the most informative positions are those where:

* the predicted result is wrong;
* the evaluation is sensitive to the parameters;
* the game result is not already obvious;
* the position represents the positions your engine actually encounters.

A position evaluated at (+10) that was won contributes little because most reasonable parameter changes still predict a win. A position evaluated at (+0.2) that was consistently lost may produce a much stronger useful gradient.

So for parameter tuning, I would emphasize **calibration errors near the decision boundary**, not simply positions where deeper search discovers a large advantage.

Search-aware learning methods such as TDLeaf were developed around a related idea: use values produced by game-tree search to improve the underlying evaluation rather than treating every encountered position identically. ([arXiv][3])

## A practical pipeline for your engine

For every selected self-play position:

```text
1. Reject illegal, terminal and tablebase-resolved positions.
2. Run quiescence search:
       qscore

3. Run normal search at the labeling budget:
       deep_score

4. Extend the search somewhat:
       verification_score

5. Record:
       FEN/features
       qscore
       deep_score
       verification_score
       game result
       ply
       game ID
       material/phase information
```

Then filter and classify:

```text
stable =
    abs(verification_score - deep_score) < 40 cp

hard =
    abs(deep_score - qscore) > 100 cp

quiet =
    not in_check
    and no obvious unresolved winning capture
    and qsearch is not producing a large tactical swing
```

Sampling:

```text
70% representative stable positions
20% hard + stable positions
10% rare position classes
```

Possible weighting:

[
w =
1 +
\alpha
\min\left(
\frac{|s_{\text{deep}}-s_{\text{NN}}|}{150},
2
\right)
]

with the total weight capped around (2)–(3) times a normal position. Large-error examples should receive more attention, but not unlimited attention.

## My main recommendation

For your engine, I would use **deeper-search surprise as a selection signal**, but not as the label-quality criterion:

[
\boxed{
\text{valuable}
===============

\text{representative}
+
\text{model disagreement}
+
\text{deep-score stability}
+
\text{learnable board features}
}
]

The best position is not necessarily one where search suddenly finds (+5). It is often one where the network says (0.0), a deeper search consistently says (+0.8), and that (+0.8) comes from positional information the network could learn.

[1]: https://arxiv.org/html/2412.17948v1 "Study of the Proper NNUE Dataset"
[2]: https://official-stockfish.github.io/docs/nnue-pytorch-wiki/docs/nnue.html "NNUE | Stockfish Docs"
[3]: https://arxiv.org/abs/cs/9901001 "[cs/9901001] TDLeaf(lambda): Combining Temporal Difference Learning with Game-Tree Search"
