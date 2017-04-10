$ondollar
$title Want/Can Inst Full Optimization

$offsymxref offsymlist offuelxref offuellist offupper
option limrow = 0, limcol = 0;
option optcr = 0.0;
option optca = 0.0;

* === SETS, PARAMETERS, AND SCALARS ===
set nodes;
set entry(nodes);
set exit(nodes);
set edges(nodes, nodes);
set desired(nodes);
set can_inst(nodes);
set a(nodes, nodes, nodes, nodes);
parameter cost(nodes);

$GDXIN indata.gdx
$LOAD nodes
$LOAD entry
$LOAD exit
$LOAD edges
$LOAD desired
$LOAD can_inst
$LOAD a
$LOAD cost
$GDXIN

* Compute the legal (alpha, beta, d) triples
* (could probably be restricted and passed as input)
set alphas(nodes);
alphas(nodes) = can_inst(nodes) + entry(nodes);
set betas(nodes);
betas(nodes) = can_inst(nodes) + exit(nodes);
set triples(nodes, nodes, nodes);
triples(alphas, betas, desired)$(not sameas(alphas, desired) and not sameas(betas, desired)) = yes;

* scalar bigcost;
* bigcost = sum(nodes, cost(nodes)) + 1.0;

* === ASSERTIONS OVER INPUT DATA ===
abort$(card(entry) <> 1) "must provide exactly 1 entry node", entry;
abort$(card(exit) < 1) "must provide at least one 1 exit/crash node", exit;

* === VARIABLES AND EQUATIONS ===
free variables
  total_cost "The total instrumentation cost",
  theta(nodes, nodes, nodes, nodes) "Farkas multipliers for entry to alpha",
  eta(nodes, nodes, nodes, nodes) "Farkas multipliers for beta to exit",
  etachi(nodes, nodes, nodes) "Farkas multipliers for joint-exit chi node",
  pi(nodes, nodes, nodes, nodes) "Farkas multipliers for alpha to d",
  mu(nodes, nodes, nodes, nodes) "Farkas multipliers for alpha to beta",
  lambda(nodes, nodes, nodes, nodes) "Farkas multipliers for d to beta"
;

binary variables
  z(nodes) "indicates if node is in the coverage set (probed)",
  s(nodes, nodes) "indicates if entry to alpha is empty",
  t(nodes, nodes) "indicates if beta to exit is empty",
  u(nodes, nodes, nodes) "indicates if alpha to d is empty",
  v(nodes, nodes, nodes) "indicates if alpha to beta is empty",
  w(nodes, nodes, nodes) "indicates if d to beta is empty"
;

equations
  cost_eq "Objective: minimize cost (covering as much of desired as possible)",
  inst_from_flow_eq(nodes, nodes, nodes) "Required instrumentation based on flow results",
  theta_eq(nodes, nodes, nodes, nodes, nodes) "Farkas equations for ...",
  theta_flow_eq(nodes, nodes, nodes, nodes),
  eta_eq(nodes, nodes, nodes, nodes, nodes) "Farkas equations for ...",
  eta_flow_eq1(nodes, nodes, nodes, nodes),
  eta_flow_eq2(nodes, nodes, nodes),
  pi_eq(nodes, nodes, nodes, nodes, nodes),
  pi_flow_eq(nodes, nodes, nodes),
  mu_eq(nodes, nodes, nodes, nodes, nodes),
  mu_flow_eq(nodes, nodes, nodes),
  lambda_eq(nodes, nodes, nodes, nodes, nodes),
  lambda_flow_eq(nodes, nodes, nodes)
;

* === ALIASES ===
alias(nodes, alpha, beta, d, I, J, K, e, x);

* === MODEL DEFINITION ===
cost_eq..
  total_cost =E= sum(nodes, cost(nodes) * z(nodes));

inst_from_flow_eq(alpha, beta, d)$triples(alpha, beta, d)..
  s(alpha, d) + t(beta, d) + u(alpha, beta, d) + v(alpha, beta, d) + w(alpha, beta, d) =G= 1 - z(d);

theta_eq(alpha, beta, d, I, J)$(triples(alpha, beta, d) and edges(I, J) and not sameas(I, d) and not sameas(J, d))..
  theta(alpha, beta, d, I) - theta(alpha, beta, d, J) =G= 0;

theta_flow_eq(alpha, beta, d, e)$(triples(alpha, beta, d) and entry(e))..
  theta(alpha, beta, d, e) - theta(alpha, beta, d, alpha) =L= 1 - 2 * s(alpha, d);

eta_eq(alpha, beta, d, I, J)$(triples(alpha, beta, d) and edges(I, J) and not sameas(I, d) and not sameas(J, d))..
  eta(alpha, beta, d, I) - eta(alpha, beta, d, J) =G= 0;

eta_flow_eq1(alpha, beta, d, x)$(triples(alpha, beta, d) and exit(x) and not sameas(x, d))..
  eta(alpha, beta, d, x) - etachi(alpha, beta, d) =G= 0;

eta_flow_eq2(alpha, beta, d)$(triples(alpha, beta, d))..
  eta(alpha, beta, d, beta) - etachi(alpha, beta, d) =L= 1 - 2 * t(beta, d);

pi_eq(alpha, beta, d, I, J)$(triples(alpha, beta, d) and edges(I, J))..
  pi(alpha, beta, d, I) - pi(alpha, beta, d, J) =G= -1 * (z(I) - a(alpha, beta, d, I) * z(I)) - (z(J) - a(alpha, beta, d, J) * z(J));

pi_flow_eq(alpha, beta, d)$triples(alpha, beta, d)..
  pi(alpha, beta, d, alpha) - pi(alpha, beta, d, d) =L= 1 - 2 * u(alpha, beta, d);

mu_eq(alpha, beta, d, I, J)$(triples(alpha, beta, d) and edges(I, J) and not sameas(I, d) and not sameas(J, d))..
  mu(alpha, beta, d, I) - mu(alpha, beta, d, J) =G= -1 * (z(I) - a(alpha, beta, d, I) * z(I)) - (z(J) - a(alpha, beta, d, J) * z(J));

mu_flow_eq(alpha, beta, d)$triples(alpha, beta, d)..
  mu(alpha, beta, d, alpha) - mu(alpha, beta, d, beta) =L= 1 - 2 * v(alpha, beta, d);

lambda_eq(alpha, beta, d, I, J)$(triples(alpha, beta, d) and edges(I, J))..
  lambda(alpha, beta, d, I) - lambda(alpha, beta, d, J) =G= -1 * (z(I) - a(alpha, beta, d, I) * z(I)) - (z(J) - a(alpha, beta, d, J) * z(J));

lambda_flow_eq(alpha, beta, d)$triples(alpha, beta, d)..
  lambda(alpha, beta, d, d) - lambda(alpha, beta, d, beta) =L= 1 - 2 * w(alpha, beta, d);

z.fx(I)$(not can_inst(I)) = 0.0;



model testModel /all/;

* === SOLVE AND PRINT ===
solve testModel using mip minimizing total_cost;

parameter result(nodes);
result(nodes) = z.l(nodes);

parameter modelStat;
modelStat = testModel.modelstat;
parameter solveStat;
solveStat = testModel.solvestat;

display z.L;
display result;
display a;
display cost;
display entry;
display exit;
display edges;
