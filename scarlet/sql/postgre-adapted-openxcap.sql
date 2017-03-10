CREATE SEQUENCE subscriber_id_seq;
CREATE TABLE subscriber (
    id numeric(11) NOT NULL DEFAULT nextval('subscriber_id_seq')  PRIMARY KEY,
    username varchar(64) NOT NULL DEFAULT '', --- bez @domain.name
    domain varchar(64) NOT NULL DEFAULT '', --- iza @
    password varchar(25) NOT NULL DEFAULT '',
    ha1 varchar(64) NOT NULL DEFAULT '',--- HTTP digest auth međurezultat H1
    CONSTRAINT user_id UNIQUE (username, domain)
);
ALTER SEQUENCE subscriber_id_seq OWNED BY subscriber.id;
CREATE INDEX username_id ON subscriber (username);

CREATE SEQUENCE xcap_id_seq;
CREATE TABLE xcap (
  id numeric(11) NOT NULL DEFAULT nextval('xcap_id_seq'),
  username varchar(66) NOT NULL,
  domain varchar(128) NOT NULL,
  doc bytea NOT NULL,---dokument
  doc_type numeric(12) NOT NULL,---mime-type
  etag varchar(64) NOT NULL, --- verzija dokumenta
  source numeric(12) NOT NULL, --- ??? sta je ovo
  doc_uri varchar(128) NOT NULL, --- document selector xpath
  port numeric(12) NOT NULL, --- ??? cemu ce port
  PRIMARY KEY  (id),
  CONSTRAINT udd_xcap UNIQUE (username,domain,doc_type,doc_uri)
);
ALTER SEQUENCE xcap_id_seq OWNED BY xcap.id;

