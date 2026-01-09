# PAF Auto-Planner Agent

## Your Mission
Analyze the provided PLAN.md file and automatically generate a complete Parallel Agent Framework (PAF) setup with many small, focused agents organized into dependency waves.

## Critical Requirements
1. **Small Task Granularity**: Break down work into VERY small tasks - each task should take 5-10 minutes
2. **1 Agent = 1 Task**: Assign exactly one agent per task (no shared tasks)
3. **Many Agents**: Aim for 5-15 agents total (more is better for parallelization)
4. **Smart Waves**: Group independent tasks in the same wave, dependent tasks in later waves
5. **Clear Dependencies**: Only create dependencies when truly necessary (data from one task needed by another)

## Context Files You MUST Read First

Before starting, read these files to understand the templates:
1. `/home/nick/Workspace/parallel-agent-framework/templates/AGENT_CHARTER_TEMPLATE.md` - Template for AGENT_CHARTER.md
2. `/home/nick/Workspace/parallel-agent-framework/templates/DEPENDENCY_DAG_TEMPLATE.md` - Template for DEPENDENCY_DAG.md
3. `/home/nick/Workspace/parallel-agent-framework/templates/AGENT_PROMPT_TEMPLATE.md` - Template for agent prompts

## Your Task

### Step 1: Read and Analyze PLAN.md
- Read the PLAN.md file at: /home/nick/Workspace/acap-native-sdk-examples/lrf-controller/PLAN.md
- Understand the overall goal and required work
- Identify all discrete pieces of work that need to be done

### Step 2: Break Down Into Small Tasks
- Decompose the work into 5-15 small, focused tasks
- Each task should be:
  - Completable in 5-10 minutes
  - Focused on ONE specific thing (analysis, design, implementation, testing, etc.)
  - Independent where possible
  - Have a clear, measurable output

**Examples of good task granularity:**
- ❌ BAD: "Implement authentication system" (too big)
- ✅ GOOD: "Design user database schema"
- ✅ GOOD: "Design login API endpoint structure"
- ✅ GOOD: "Design password hashing strategy"
- ✅ GOOD: "Design session management approach"

### Step 3: Identify Dependencies
- Map which tasks depend on outputs from other tasks
- Only create dependencies when Task B truly NEEDS the output of Task A
- Group independent tasks together in early waves
- Create as much parallelism as possible

### Step 4: Generate PAF Files
Generate the following files in the `/home/nick/Workspace/acap-native-sdk-examples/lrf-controller/.paf/` directory:

#### A. AGENT_CHARTER.md
- Follow the template structure exactly
- Include all agents organized by wave
- Provide clear role and task for each agent
- Set appropriate timeouts (5-15 minutes per agent)

#### B. DEPENDENCY_DAG.md
- Follow the template structure exactly
- Map all dependencies clearly
- Show wave structure
- Include visualization diagram

#### C. Individual Agent Prompts (`.paf/prompts/AGENT_<ID>_PROMPT.md`)
- Create one prompt file per agent
- Follow the AGENT_PROMPT_TEMPLATE.md structure
- Be VERY specific about:
  - Context files to read (from the project)
  - Exact task steps
  - Expected output format
  - Time budget

## Output Format

You must create the following files in the project directory:

```
/home/nick/Workspace/acap-native-sdk-examples/lrf-controller/.paf/
├── AGENT_CHARTER.md
├── DEPENDENCY_DAG.md
└── prompts/
    ├── AGENT_A1_PROMPT.md
    ├── AGENT_A2_PROMPT.md
    ├── AGENT_A3_PROMPT.md
    └── ... (one per agent)
```

## Important Guidelines

### Task Decomposition Strategy
1. **Analysis Tasks**: What exists? What's the current state?
2. **Design Tasks**: How should it work? What's the approach?
3. **Implementation Tasks**: What code changes are needed?
4. **Testing Tasks**: How do we verify it works?
5. **Documentation Tasks**: What docs need updating?

### Wave Organization Strategy
- **Wave 1**: All independent analysis/investigation tasks
- **Wave 2**: Design tasks that need analysis results
- **Wave 3**: Implementation planning that needs design
- **Wave 4**: Testing/validation planning (if needed)

### Context File Selection
For each agent, identify the SPECIFIC files they need to read:
- Source code files relevant to their task
- Configuration files
- Documentation
- Previous agent findings (if dependent)

**DO NOT** give agents access to the entire codebase - be surgical.

## Example Task Breakdown

**Original Plan:** "Add user authentication"

**Good Breakdown (10 agents, 3 waves):**

**Wave 1 (Independent Analysis):**
- A1: Analyze current user data models
- A2: Research auth libraries and frameworks available
- A3: Analyze existing API endpoint patterns
- A4: Review current security practices in codebase

**Wave 2 (Design, depends on Wave 1):**
- A5: Design user/session database schema (needs A1)
- A6: Design authentication API endpoints (needs A2, A3)
- A7: Design password security strategy (needs A2, A4)
- A8: Design session management approach (needs A2, A4)

**Wave 3 (Implementation Planning, depends on Wave 2):**
- A9: Create backend implementation task list (needs A5, A6, A7, A8)
- A10: Create frontend integration task list (needs A6, A8)

## Success Criteria
- [ ] Generated AGENT_CHARTER.md with 5-15 agents
- [ ] Generated DEPENDENCY_DAG.md with clear wave structure
- [ ] Generated one AGENT_*_PROMPT.md per agent
- [ ] Each agent has a small, focused task (5-10 min)
- [ ] Dependencies are minimal and necessary
- [ ] Maximum parallelization achieved
- [ ] All files follow template formats exactly

## Time Budget
30 minutes maximum. Focus on creating a comprehensive breakdown.

---
**BEGIN WORK NOW.** Start by reading the PLAN.md file at /home/nick/Workspace/acap-native-sdk-examples/lrf-controller/PLAN.md, then generate all PAF files.
