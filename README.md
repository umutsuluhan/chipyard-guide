# ECE 462/562 (University of Arizona) - Chipyard Installation Guide and Tutorial

This repository provides the Chipyard/Rocket environment used for
ECE 462/562 Computer Architecture.

(Thanks to Umut for creating the original version of this tutorial!)

## Requirements

Install:

1. Git
2. Docker Desktop: [Docker for Windows](https://docs.docker.com/desktop/setup/install/windows-install/) (requires WSL 2); [Docker for Linux](https://docs.docker.com/engine/install/)

## Setup

Clone this repository:

```bash
git clone https://github.com/tosiron/ua-chipyard.git
cd ua-chipyard
```

Download the course environment:

```bash
docker compose pull
```

Start Chipyard:

```bash
docker compose run --rm chipyard
```

Inside the container, verify installation:

```bash
/workspace/test-install.sh
```

You should see:

```text
Environment: PASS
```

### If You Cannot Run Docker

If your computer does not have enough memory for the container, or you work on a
server where Docker is unavailable, Chipyard can be built natively from source
instead. It takes 1-3 hours and about 45 GB, and needs no administrator rights.

See [Native Installation (No Docker)](/docs/native-install.md).

## Using Course and Custom Chipyard Configurations

The Docker image contains the fixed Chipyard environment and toolchain. Course configuration files are **not built into the Docker image**. Instead, they are provided through this Git repository and automatically made available to Chipyard when the container starts.

### Getting New Course Configurations

When a new configuration is released, update your local repository:

```bash
git pull
```

Then start the environment normally:

```bash
docker compose run --rm chipyard
```

The startup script automatically makes all instructor-provided `.scala` files in:

```text
course/configs/
```

available to Chipyard. **You do not need to rebuild or re-download the Docker image when a new course configuration is released.**

For example, if `git pull` adds:

```text
course/configs/CourseCacheConfig.scala
```

you can build it inside the container with:

```bash
cd /workspace/chipyard/sims/verilator
make CONFIG=CourseCacheConfig
```

### Adding Your Own Configuration

Student-created or modified Scala configurations should be stored somewhere under:

```text
student-work/
```

For example:

```text
student-work/lab4/MyCacheConfig.scala
```

Do **not** modify the instructor files in `course/configs/`.

When the container starts, the course setup automatically makes student `.scala` configuration files under `student-work/` available to Chipyard.

After adding a new configuration, restart the container:

```bash
docker compose run --rm chipyard
```

Then build your configuration normally:

```bash
cd /workspace/chipyard/sims/verilator
make CONFIG=MyCacheConfig
```

You can check that Chipyard recognizes your configuration with:

```bash
make find-configs
```

### When Do I Need a New Docker Image?

Normally, you don't.

| Change                                 | What to do                                                 |
| -------------------------------------- | ---------------------------------------------------------- |
| New course configuration               | `git pull`                                                 |
| Updated course configuration           | `git pull`                                                 |
| New assignment or benchmark            | `git pull`                                                 |
| Your own Scala configuration           | Put it in `student-work/` and restart the container        |
| Updated Chipyard/toolchain environment | `git pull` followed by `docker compose pull` if instructed |

Only download a new Docker image when explicitly instructed to do so.

> **If you are not using Docker and installed Chipyard natively on Linux:** wherever this section says to start or restart the container, run `source scripts/native-activate.sh` instead. A `git pull` is enough for configuration updates. See [Native Installation](/docs/native-install.md).

Now, you may continue to:

[Getting Started](/docs/getting-started.md)
