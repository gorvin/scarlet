/*
sudo /etc/init.d/postgresql-8.4 stop
sudo update-rc.d -f postgresql-8.4 remove

/usr/lib/postgresql/8.4/bin/initdb -D /home/igor/pgsql/data

nano ~/pgsql/data/postgresql.conf
 logging_collector = on
 log_directory = 'pg_log'

/usr/lib/postgresql/8.4/bin/postgres -k /tmp -D ~/pgsql/data
psql -h /tmp -c "CREATE LANGUAGE 'plpgsql';" postgres
psql -h /tmp -v do_setup=1 -f postgre-create-tables.sql postgres

INSERT INTO xcaptree (etag, auid, xid, filename, document, tstamp.created, tstamp.modified) \
 VALUES ('a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11', 'xcap-app1', 'myname', 'app1.cfg', 'sadrzaj', 'now', 'now');

/usr/lib/postgresql/8.4/bin/pg_ctl stop -D ~/pgsql/data

TODO: U PostgreSQL treba username za program excoserver
      excoserver ce ucitati username i password iz svog konfiguracionog fajla
*/

CREATE LANGUAGE 'plpgsql';

CREATE OR REPLACE FUNCTION excotables_setup (
) RETURNS boolean AS $$
<< outerblock >>
BEGIN
    DROP TABLE IF EXISTS xcaptree CASCADE;
    DROP TABLE IF EXISTS xcapusers CASCADE;
    DROP TYPE IF EXISTS access_timestamp_type CASCADE;

    /* NOTE: iskoristi PostgreSql Special Date/Time Input: "now", ili funkciju CURRENT_TIMESTAMP */
    CREATE TYPE access_timestamp_type AS (
        created timestamp(0) with time zone, --- seconds,
        modified timestamp(0) with time zone --- seconds
    );

    CREATE TABLE xcapusers (
        username text primary key, --- 'user@domain'
        digest text not null --- password for Basic or hash for Digest HTTP authentication
    );

    -- Ova tabela moze biti prazna prije prvog pokretanja servera
    CREATE TABLE xcaptree (
        xid text not null, --- 'user@domain', NOTE: '@domain' je za global
        auid text not null,
        filename text not null, --- document name
        primary key (xid, auid, filename),
        document text not null, --- document content
        etag uuid not null, --- uuid verzije dokumenta / etag
        tstamp access_timestamp_type not null
    );
    return true;
END;
$$ LANGUAGE plpgsql;

/* Da bih bio siguran da izvršavam skript samo ako znam šta radim napravio sam da se za izvrsavanje
   mora eksplicitno pri pozivu skripta postaviti varijabla do_setup na TRUE (true, 1, 2, .. s) npr.
        psql -h /tmp -v do_setup=1 -f postgre-create-tables.sql postgres
*/
select excotables_setup() where (:do_setup::boolean);
