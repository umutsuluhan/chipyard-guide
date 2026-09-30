> **If you are not using Docker and installed Chipyard natively on Linux:** skip `docker compose run --rm chipyard` and run `source scripts/native-activate.sh` from your clone instead. In every command in this guide, replace `/workspace` with `$WS`. For example, `cd /workspace/chipyard` becomes `cd $WS/chipyard`. See [Native Installation](/docs/native-install.md), Sections 5 and 6.

Make sure you have the updated repo version:

```
git pull
```

Start the Chipyard container:
```
docker compose run --rm chipyard
```

Load the Chipyard environment:
```
cd /workspace/chipyard
source env.sh
```

Compile BaselineConfig:
```
cd /workspace/chipyard/sims/verilator
make CONFIG=BaselineConfig
```

Run the provided ISP benchmark:

` quiz-run isp ` or the full path ` /workspace/course-scripts/quiz-run.sh isp ` should work with up-to-date repos.

This might take several minutes depending on your computer speed. The output should provide performance statistics for the different phases of isp_bench:  
gain  
vfilter  
downscale  
bgmodel  
detect  

Now, you are ready to answer Quiz 3 questions based on these stats.
