# Parallel Agent Framework (PAF)
**Version:** 2.0
**Created:** 2026-01-08
**Purpose:** Generalized framework for coordinating parallel Claude Code agents to complete complex tasks

---

## 📖 Overview

This framework enables efficient parallel task execution using multiple Claude Code agents working concurrently. It provides concrete spawning mechanics, synchronization patterns, and error recovery for real-world parallel agent orchestration.

**When to Use This Framework:**
- Tasks can be decomposed into 3+ independent subtasks
- Each subtask requires specialized exploration or context isolation
- Total work exceeds what a single context window can efficiently handle
- Time-to-completion benefits from parallelization

**When NOT to Use:**
- Simple sequential tasks (use single agent)
- Fewer than 3 independent subtasks
- Tasks with strict sequential ordering that can't be parallelized
- Quick tasks completable in <10 minutes by single agent

---

## 🏗️ Architecture Patterns

### Pattern 1: Orchestrator-Worker (Recommended Default)
**Use when:** Clear task delegation, centralized control needed
```
                    ┌─────────────┐
                    │ Coordinator │
                    └──────┬──────┘
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
      ┌─────────┐     ┌─────────┐     ┌─────────┐
      │ Agent 1 │     │ Agent 2 │     │ Agent 3 │
      └─────────┘     └─────────┘     └─────────┘
           │               │               │
           ▼               ▼               ▼
      A1_FINDINGS.md  A2_FINDINGS.md  A3_FINDINGS.md
```

### Pattern 2: Staged Waves (For Dependencies)
**Use when:** Some agents depend on others' outputs
```
  Wave 1 (Independent)     Wave 2 (Dependent)      Wave 3 (Synthesis)
  ┌────┐ ┌────┐ ┌────┐         ┌────┐                  ┌────┐
  │ A1 │ │ A2 │ │ A3 │   ───►  │ A4 │      ───►        │ A5 │
  └────┘ └────┘ └────┘         └────┘                  └────┘
     │      │      │              │                       │
     └──────┴──────┴──────────────┴───────────────────────┘
                              MERGED_FINDINGS.md
```

---

## 🔧 Claude Code Spawning Mechanics

### Method 1: Background Processes (Recommended)
```bash
# Coordinator spawns agents as background processes
claude -p "$(cat AGENT_A1_PROMPT.md)" --output-file A1_FINDINGS.md &
PID_A1=$!

claude -p "$(cat AGENT_A2_PROMPT.md)" --output-file A2_FINDINGS.md &
PID_A2=$!

claude -p "$(cat AGENT_A3_PROMPT.md)" --output-file A3_FINDINGS.md &
PID_A3=$!

# Wait for all agents to complete
wait $PID_A1 $PID_A2 $PID_A3

# Signal completion
echo "ALL_AGENTS_COMPLETE" > .agent_sync
```

### Method 2: Subshell with Timeout
```bash
# Spawn with timeout (prevents hung agents)
timeout 600 claude -p "$(cat AGENT_A1_PROMPT.md)" > A1_FINDINGS.md 2>&1 &
PID_A1=$!

# Check exit status after wait
wait $PID_A1
if [ $? -eq 124 ]; then
    echo "TIMEOUT" > A1_STATUS.md
else
    echo "COMPLETE" > A1_STATUS.md
fi
```

### Method 3: GNU Parallel (For Many Agents)
```bash
# Create agent prompts as separate files
parallel --jobs 5 --timeout 600 \
    'claude -p "$(cat {})" > {.}_FINDINGS.md 2>&1' \
    ::: AGENT_A*.md
```

---

## 📁 File Structure (Race-Condition Safe)

Each agent writes to its **own isolated files** to prevent write conflicts:

```
project/
├── PLAN.md                      # Original task (read-only)
├── PARALLEL_AGENT_FRAMEWORK.md  # This framework (read-only)
├── .paf/                        # PAF working directory
│   ├── AGENT_CHARTER.md         # Agent roster and rules (coordinator writes)
│   ├── DEPENDENCY_DAG.md        # Task dependencies (coordinator writes)
│   │
│   ├── prompts/                 # Agent prompt files
│   │   ├── AGENT_A1_PROMPT.md
│   │   ├── AGENT_A2_PROMPT.md
│   │   └── AGENT_A3_PROMPT.md
│   │
│   ├── findings/                # Agent outputs (each agent writes own file)
│   │   ├── A1_FINDINGS.md
│   │   ├── A2_FINDINGS.md
│   │   └── A3_FINDINGS.md
│   │
│   ├── status/                  # Completion signals
│   │   ├── A1_STATUS.md         # COMPLETE | FAILED | TIMEOUT
│   │   ├── A2_STATUS.md
│   │   └── A3_STATUS.md
│   │
│   ├── MERGED_FINDINGS.md       # Coordinator merges all findings
│   └── FINAL_PLAN.md            # Synthesized implementation plan
```

---

## 📊 Dependency DAG Specification

Define task dependencies explicitly. Coordinator spawns in waves based on this DAG.

### DEPENDENCY_DAG.md Format
```markdown
# Task Dependency Graph

## Wave 1 (Independent - Spawn Immediately)
| Agent | Task | Depends On | Blocks | Timeout |
|-------|------|------------|--------|---------|
| A1 | Audit existing code | None | A4 | 10min |
| A2 | Analyze metrics | None | A4, A5 | 10min |
| A3 | Review documentation | None | A5 | 5min |

## Wave 2 (Dependent - Spawn After Wave 1)
| Agent | Task | Depends On | Blocks | Timeout |
|-------|------|------------|--------|---------|
| A4 | Design new architecture | A1, A2 | A6 | 15min |
| A5 | Write test strategy | A2, A3 | A6 | 10min |

## Wave 3 (Synthesis - Spawn After Wave 2)
| Agent | Task | Depends On | Blocks | Timeout |
|-------|------|------------|--------|---------|
| A6 | Create implementation plan | A4, A5 | None | 10min |

## Execution Order
Wave 1: A1, A2, A3 (parallel) → wait all complete
Wave 2: A4, A5 (parallel) → wait all complete  
Wave 3: A6 → final synthesis
```

### Wave Execution Script
```bash
#!/bin/bash
set -e

PAF_DIR=".paf"
PROMPTS="$PAF_DIR/prompts"
FINDINGS="$PAF_DIR/findings"
STATUS="$PAF_DIR/status"

spawn_wave() {
    local wave_name=$1
    shift
    local agents=("$@")
    local pids=()
    
    echo "=== Starting $wave_name ==="
    
    for agent in "${agents[@]}"; do
        timeout 600 claude -p "$(cat $PROMPTS/AGENT_${agent}_PROMPT.md)" \
            > "$FINDINGS/${agent}_FINDINGS.md" 2>&1 &
        pids+=($!)
        echo "Spawned $agent (PID: ${pids[-1]})"
    done
    
    # Wait for all agents in wave
    local failed=()
    for i in "${!agents[@]}"; do
        wait ${pids[$i]} 2>/dev/null
        exit_code=$?
        if [ $exit_code -eq 0 ]; then
            echo "COMPLETE" > "$STATUS/${agents[$i]}_STATUS.md"
        elif [ $exit_code -eq 124 ]; then
            echo "TIMEOUT" > "$STATUS/${agents[$i]}_STATUS.md"
            failed+=("${agents[$i]}")
        else
            echo "FAILED:$exit_code" > "$STATUS/${agents[$i]}_STATUS.md"
            failed+=("${agents[$i]}")
        fi
    done
    
    if [ ${#failed[@]} -gt 0 ]; then
        echo "WARNING: Failed agents: ${failed[*]}"
        return 1
    fi
    
    echo "=== $wave_name Complete ==="
    return 0
}

# Execute waves in order
mkdir -p "$FINDINGS" "$STATUS"

spawn_wave "Wave 1" A1 A2 A3
spawn_wave "Wave 2" A4 A5  
spawn_wave "Wave 3" A6

echo "=== All Waves Complete ==="
```

---

## 🎯 Agent Roles & Context Budgets

### Coordinator Agent
**Role:** Orchestrator, synthesizer, decision-maker
**Context Budget:** Full project context + all agent findings
**Responsibilities:**
1. Read PLAN.md and this framework
2. Decompose into 3-7 parallel subtasks (optimal range)
3. Create AGENT_CHARTER.md and DEPENDENCY_DAG.md
4. Generate agent prompt files with strict context limits
5. Execute wave spawning script
6. Monitor status files for completion/failure
7. Merge findings and synthesize final plan
8. Handle failed agents (retry or skip with note)

### Specialist Agents (3-7 agents optimal)
**Role:** Domain experts with isolated context
**Context Budget:** ONLY files listed in their prompt (minimize context stuffing)
**Responsibilities:**
1. Read ONLY the files specified in prompt
2. Execute assigned task within strict boundaries
3. Output findings in structured format to own file
4. Do NOT read other agents' findings (isolation)
5. Complete within timeout

**Critical:** Each agent operates in isolation. They do NOT coordinate with peers during execution. All synthesis happens at the coordinator level after completion.

---

## 📋 Agent Prompt Template

```markdown
# Agent [ID]: [Role Name]

## Your Mission
[One sentence task description]

## Context Files (READ ONLY THESE)
- `path/to/file1.py` - [why this file matters]
- `path/to/file2.md` - [why this file matters]
- `path/to/directory/` - [what to look for]

**DO NOT READ:** Other agent findings, unrelated source files, or files not listed above.

## Your Task
1. [Specific action 1 with measurable output]
2. [Specific action 2 with measurable output]
3. [Specific action 3 with measurable output]

## Output Format (STRICTLY FOLLOW)
Your output MUST follow this exact structure:

```
# [Agent ID] Findings

## Executive Summary
[2-3 sentences: what you found, what you recommend]

## Key Findings
1. **[Finding Title]**: [Evidence and details]
2. **[Finding Title]**: [Evidence and details]
3. **[Finding Title]**: [Evidence and details]

## Recommendations
1. [Actionable recommendation with rationale]
2. [Actionable recommendation with rationale]

## Files Analyzed
- `path/to/file` - [what you found]

## Blockers or Uncertainties
- [Any issues that need coordinator attention]

## Confidence Level
[HIGH | MEDIUM | LOW] - [Brief justification]
```

## Success Criteria
- [ ] All files in context list were analyzed
- [ ] Findings are specific with file/line references where applicable
- [ ] Recommendations are actionable
- [ ] Output follows the exact format above

## Time Budget
[X] minutes maximum. Focus on highest-value findings first.

---
BEGIN WORK NOW. Start by reading the context files, then produce your findings.
```

---

## 🔄 Workflow Phases

### Phase 0: Initialization (Coordinator)
**Duration:** 5-10 minutes
**Actions:**
1. Read PLAN.md and understand full task scope
2. Identify 3-7 independent subtasks
3. Map dependencies between tasks → create DEPENDENCY_DAG.md
4. Create .paf/ directory structure
5. Create AGENT_CHARTER.md
6. Generate agent prompt files in .paf/prompts/
7. **CHECKPOINT:** Show charter and DAG to user for approval

**Output:**
```
.paf/
├── AGENT_CHARTER.md
├── DEPENDENCY_DAG.md
└── prompts/
    ├── AGENT_A1_PROMPT.md
    ├── AGENT_A2_PROMPT.md
    └── ...
```

### Phase 1: Wave Execution (Coordinator)
**Duration:** Variable (sum of wave timeouts)
**Actions:**
1. Execute wave spawning script
2. For each wave:
   - Spawn all agents in wave as background processes
   - Wait for all to complete (or timeout)
   - Check status files for failures
   - If critical agent fails: retry once or abort with user notification
3. Proceed to next wave only after current wave completes

**Error Handling:**
```bash
# Retry failed agent once
if [ "$(cat $STATUS/A1_STATUS.md)" != "COMPLETE" ]; then
    echo "Retrying A1..."
    timeout 600 claude -p "$(cat $PROMPTS/AGENT_A1_PROMPT.md)" \
        > "$FINDINGS/A1_FINDINGS.md" 2>&1
    if [ $? -eq 0 ]; then
        echo "COMPLETE" > "$STATUS/A1_STATUS.md"
    else
        echo "FAILED_RETRY" > "$STATUS/A1_STATUS.md"
    fi
fi
```

### Phase 2: Validation (Coordinator)
**Duration:** 2-5 minutes
**Actions:**
1. Check all status files for completion
2. Validate each findings file against expected format:
   - Has Executive Summary section
   - Has Key Findings section
   - Has Recommendations section
   - Has Confidence Level
3. Flag malformed outputs for manual review
4. Create validation report

**Validation Script:**
```bash
validate_findings() {
    local file=$1
    local required=("Executive Summary" "Key Findings" "Recommendations" "Confidence Level")
    local missing=()
    
    for section in "${required[@]}"; do
        if ! grep -q "## $section" "$file"; then
            missing+=("$section")
        fi
    done
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo "INVALID: Missing sections: ${missing[*]}"
        return 1
    fi
    echo "VALID"
    return 0
}
```

### Phase 3: Synthesis (Coordinator)
**Duration:** 10-15 minutes
**Actions:**
1. Read all valid agent findings
2. Identify conflicts between agent recommendations
3. Resolve conflicts (prefer higher-confidence findings, or flag for user)
4. Merge non-conflicting recommendations
5. Create MERGED_FINDINGS.md with:
   - Combined findings organized by theme
   - Conflict resolution notes
   - Gaps or areas needing more investigation
6. Synthesize into FINAL_PLAN.md

**MERGED_FINDINGS.md Structure:**
```markdown
# Merged Agent Findings

## Synthesis Metadata
- Agents: A1, A2, A3, A4, A5
- Completed: A1, A2, A3, A5
- Failed: A4 (timeout, findings partial)
- Validation: All passed format check

## Consolidated Findings

### Theme 1: [Topic]
**From A1:** [Finding summary]
**From A3:** [Finding summary]
**Synthesis:** [Combined insight]

### Theme 2: [Topic]
**From A2:** [Finding summary]
**From A5:** [Finding summary]  
**Conflict:** A2 recommends X, A5 recommends Y
**Resolution:** [Coordinator decision with rationale]

## Gaps Identified
- [Area not covered by any agent]
- [Area with low-confidence findings]

## Action Items
1. [Prioritized action from synthesis]
2. [Prioritized action from synthesis]
```

### Phase 4: Handoff (Coordinator)
**Duration:** 2-5 minutes
**Actions:**
1. Create FINAL_PLAN.md with implementation steps
2. Archive all .paf/ artifacts
3. Present to user with:
   - Executive summary of findings
   - Recommended next steps
   - Any unresolved issues or gaps
   - Links to detailed findings for reference

---

## ⚠️ Error Recovery

### Agent Timeout
```bash
# Detection: exit code 124 from timeout command
# Recovery: Retry once with same prompt, or skip and note in synthesis
```

### Agent Produces Malformed Output
```bash
# Detection: Validation script fails
# Recovery: 
#   1. Check if partial findings are usable
#   2. Retry with simplified prompt if time allows
#   3. Note gap in synthesis
```

### Agent Hangs (No Output)
```bash
# Detection: Status file never written, PID still running after timeout
# Recovery:
#   1. Kill process: kill -9 $PID
#   2. Mark as KILLED in status
#   3. Retry or skip
```

### Dependency Chain Failure
```bash
# Detection: Agent in Wave N depends on failed agent in Wave N-1
# Recovery:
#   1. Skip dependent agent
#   2. Note in synthesis that analysis is incomplete
#   3. Offer user option to re-run with fixed Wave N-1
```

---

## 🛠️ Best Practices

### Task Decomposition
✅ **DO:**
- Create 3-7 independent tasks (optimal for Claude Code)
- Each task should take 5-15 minutes
- Define clear inputs (specific files) and outputs (findings format)
- Front-load independent tasks, chain dependent ones

❌ **DON'T:**
- Create >7 agents (coordination overhead exceeds benefit)
- Create chains where every agent depends on the previous
- Give agents vague mandates like "investigate the codebase"
- Let agents read unlimited context

### Context Management
✅ **DO:**
- Explicitly list files each agent should read
- Keep agent context to <50 files where possible
- Include only files relevant to that agent's specific task
- Provide brief annotations for why each file matters

❌ **DON'T:**
- Give agents access to "the whole repo"
- Let agents read other agents' findings (breaks isolation)
- Include files "just in case they're useful"

### Output Quality
✅ **DO:**
- Enforce strict output format via prompt
- Validate outputs before synthesis
- Require confidence levels on findings
- Ask for specific file/line references

❌ **DON'T:**
- Accept free-form prose outputs
- Skip validation step
- Trust all findings equally regardless of confidence
- Synthesize before validating

---

## 📋 Quick Start Checklist

```markdown
## Coordinator Checklist

### Phase 0: Setup
- [ ] Read PLAN.md completely
- [ ] Identify 3-7 subtasks
- [ ] Map dependencies → DEPENDENCY_DAG.md
- [ ] Create .paf/ directory structure
- [ ] Write AGENT_CHARTER.md
- [ ] Generate all agent prompts in .paf/prompts/
- [ ] **USER CHECKPOINT:** Get approval on charter and DAG

### Phase 1: Execution  
- [ ] Run wave spawning script
- [ ] Monitor for timeouts/failures
- [ ] Retry failed agents once (if time allows)
- [ ] Confirm all waves complete

### Phase 2: Validation
- [ ] Check all status files
- [ ] Validate findings format
- [ ] Flag malformed outputs

### Phase 3: Synthesis
- [ ] Read all valid findings
- [ ] Identify and resolve conflicts
- [ ] Create MERGED_FINDINGS.md
- [ ] Synthesize FINAL_PLAN.md

### Phase 4: Handoff
- [ ] Present summary to user
- [ ] Provide links to detailed findings
- [ ] Note any gaps or failures
- [ ] Archive .paf/ directory
```

---

## 🚫 Anti-Patterns

| Anti-Pattern | Problem | Solution |
|--------------|---------|----------|
| **Shared File Writing** | Race conditions corrupt data | Each agent writes own file |
| **Unlimited Context** | Agents get confused, slow | Explicit context file lists |
| **No Timeouts** | Hung agents block everything | Always use `timeout` command |
| **No Validation** | Garbage in, garbage out | Validate format before synthesis |
| **Deep Dependency Chains** | Serializes execution | Front-load independent work |
| **Peer Coordination** | Agents waiting on each other | Coordinator handles all synthesis |
| **>7 Agents** | Coordination overhead explodes | Keep to 3-7 agents |
| **No Error Recovery** | One failure kills entire run | Retry logic + graceful degradation |

---

## 📚 References

- **Anthropic Multi-Agent Research System:** https://www.anthropic.com/engineering/multi-agent-research-system
- **Multi-Agent Collaboration Patterns:** https://aws.amazon.com/blogs/machine-learning/multi-agent-collaboration-patterns-with-strands-agents-and-amazon-nova/
- **Scaling Agent Systems:** https://arxiv.org/html/2512.08296v1

---

**Framework Status:** Ready for use
**Next Steps:** 
1. Read your PLAN.md
2. Create .paf/AGENT_CHARTER.md
3. Create .paf/DEPENDENCY_DAG.md  
4. Generate agent prompts
5. Get user approval
6. Execute waves
