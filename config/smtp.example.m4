_smtp_define(smtp_:
  server: smtp.gmail.com
  username: <user>@gmail.com
  password: !secret smtp_password
  from: <sender>@gmail.com
  to: <recipient>@gmail.com
  cas: smtp.gmail.com-cas.pem
)
