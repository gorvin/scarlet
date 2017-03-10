-- http://stackoverflow.com/questions/13405071/sqlite-converts-all-unicode-characters-into-ansi
-- za ispravno prikazivanje UTF-8 znakova u windows command promptu izvrši komandu 
-- chcp 65001
-- tools\sqlite3.exe Win32\Debug\xca.sqlite < sql-scripts\sqlite3-create-tables.sql

PRAGMA encoding="UTF-8";

DROP TABLE IF EXISTS xcaptree;
DROP TABLE IF EXISTS xcapusers;

CREATE TABLE xcapusers (
    username TEXT primary key, --- 'user@domain'
    digest TEXT not null --- password for Basic or hash for Digest HTTP authentication
);

-- Ova tabela moze biti prazna prije prvog pokretanja servera
CREATE TABLE xcaptree (
    xid TEXT not null, --- 'user@domain', NOTE: '@domain' je za global
    auid TEXT not null,
    filename TEXT not null, --- document name
    document TEXT not null, --- document content
    etag TEXT not null, --- uuid verzije dokumenta / etag
    created INTEGER not null, --- seconds
    modified INTEGER not null, --- seconds
    primary key (xid, auid, filename)
);

INSERT INTO xcapusers (username, digest) VALUES ('myname@localdomain', 'mypass');
INSERT INTO xcapusers (username, digest) VALUES ('mš1yname@localdomain', 'шифра');
INSERT INTO xcaptree (etag, auid, xid, filename, document, created, modified) VALUES ('a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11', 'xcap-app1', 'myname@localdomain', 'app1.cfg', 'sadrzaj', 'now', 'now');

SELECT * FROM xcapusers;
