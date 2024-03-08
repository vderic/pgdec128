psql -a -f test.sql &> test.out
diff test.out test.good
